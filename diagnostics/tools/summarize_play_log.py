"""Summarize bounded Explore JSONL files; never infer unrecorded game behavior."""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import sys

MAX_FILE_BYTES = 8 * 1024 * 1024
MAX_LINE_BYTES = 65536
MAX_RECORDS = 200000
ACTIVE = {'ACTIVE', 'ENTERING'}
MODEL_FAILURES = {'WRITE_FAILED', 'READ_FAILED', 'OWNERSHIP_LOST', 'TARGET_CHANGED',
                  'INVALID_TARGET', 'MODEL_CALLBACK_ERROR'}


def text(value, limit=160):
    return isinstance(value, str) and 0 < len(value) <= limit


def number(value):
    return type(value) in (int, float) and math.isfinite(value) and 0 <= value <= 1e15


def integer(value):
    return type(value) is int and 0 <= value <= 2**53 - 1


def invalid_constant(_):
    raise ValueError('non-finite JSON value')


def summarize(paths):
    if not 1 <= len(paths) <= 16:
        raise ValueError('provide 1..16 play log files')
    groups = {}
    report = {'schema': 1, 'files': len(paths), 'invalid_lines': 0, 'duplicates': 0,
              'conflicts': 0, 'ignored_records': 0, 'runs': []}
    records = 0
    for path in map(Path, paths):
        header = {}
        consumed = 0
        try:
            with path.open('rb') as source:
                while True:
                    line = source.readline(MAX_LINE_BYTES + 1)
                    if not line:
                        break
                    consumed += len(line)
                    if consumed > MAX_FILE_BYTES:
                        raise ValueError('play log exceeds file byte limit')
                    if len(line) > MAX_LINE_BYTES:
                        raise ValueError('play log exceeds line byte limit')
                    if not line.strip():
                        continue
                    records += 1
                    if records > MAX_RECORDS:
                        raise ValueError('play log record limit exceeded')
                    try:
                        row = json.loads(line, parse_constant=invalid_constant)
                        if not isinstance(row, dict):
                            raise ValueError('record must be an object')
                        kind = row.get('kind')
                        if kind == 'header':
                            header = row
                            continue
                        if kind not in {'transition', 'sample'}:
                            report['ignored_records'] += 1
                            continue
                        session = row.get('session', header.get('session'))
                        run = row.get('run', header.get('run') if header.get('session') == session else None) or 'legacy'
                        stamp = row.get('lua_clock_s')
                        if not text(session) or not text(run) or not number(stamp):
                            raise ValueError('invalid session/run/time')
                        if kind == 'transition':
                            seq, channel, state = row.get('event_seq'), row.get('channel'), row.get('to')
                            previous, dropped = row.get('from'), row.get('dropped_total', 0)
                            if (row.get('schema') != 1 or not integer(seq) or seq == 0
                                    or channel not in {'camera', 'model'} or not text(state, 96)
                                    or (previous is not None and not text(previous, 96)) or not integer(dropped)):
                                raise ValueError('invalid transition')
                            fingerprint = hashlib.sha256(json.dumps(row, sort_keys=True,
                                separators=(',', ':'), allow_nan=False).encode()).hexdigest()
                        else:
                            state = row.get('state')
                            if row.get('schema') not in (None, 1) or not text(state, 96):
                                raise ValueError('invalid sample')
                        key = (session, run)
                        if key not in groups:
                            groups[key] = {'events': {}, 'samples': {}, 'blocked': set(), 'dropped': 0}
                        group = groups[key]
                        if kind == 'transition':
                            group['dropped'] = max(group['dropped'], dropped)
                            if seq in group['blocked']:
                                continue
                            old = group['events'].get(seq)
                            if old:
                                if old['fingerprint'] == fingerprint:
                                    report['duplicates'] += 1
                                else:
                                    del group['events'][seq]
                                    group['blocked'].add(seq)
                                    report['conflicts'] += 1
                                continue
                            context = row.get('context')
                            binding = context.get('binding') if isinstance(context, dict) else None
                            group['events'][seq] = {'fingerprint': fingerprint, 'time': stamp,
                                'channel': channel, 'state': state, 'previous': previous,
                                'binding': binding if text(binding) else None}
                        else:
                            samples = group['samples']
                            if stamp in samples and samples[stamp] != state:
                                samples[stamp] = None  # Same timestamp, indeterminate order.
                            else:
                                samples[stamp] = state
                            stats = row.get('events')
                            if isinstance(stats, dict) and integer(stats.get('dropped_total')):
                                group['dropped'] = max(group['dropped'], stats['dropped_total'])
                    except (ValueError, TypeError, OverflowError, RecursionError, UnicodeError):
                        report['invalid_lines'] += 1
        except OSError as exc:
            raise ValueError('cannot read play log: ' + str(exc)) from exc
    for (session, run), group in sorted(groups.items()):
        events, samples = group['events'], group['samples']
        fallbacks, failures, waits, changes, states = Counter(), Counter(), Counter(), Counter(), Counter()
        times = [e['time'] for e in events.values()]
        for e in events.values():
            state = e['state']
            if e['channel'] == 'camera' and e['previous'] in ACTIVE and state not in ACTIVE | {'OFF'}:
                fallbacks[state] += 1
            if e['channel'] == 'model':
                if state in MODEL_FAILURES or state.startswith('RELEASE_'):
                    failures[state] += 1
                elif state.startswith('WAIT'):
                    waits[e['binding'] or state] += 1
        previous = None
        for stamp, state in sorted(samples.items()):
            if state is None:
                previous = None
                continue
            times.append(stamp)
            states[state] += 1
            if previous is not None and state != previous:
                changes[state] += 1
            previous = state
        seqs = sorted(events)
        report['runs'].append({'session': session, 'run': run,
            'evidence': 'recorded_transitions' if events else 'sampled_states_only' if times else 'insufficient_data',
            'transition_count': len(events), 'sample_count': sum(states.values()),
            'observed_start_s': min(times) if times else None,
            'observed_end_s': max(times) if times else None,
            'fallbacks': dict(sorted(fallbacks.items())), 'model_failures': dict(sorted(failures.items())),
            'model_waits': dict(sorted(waits.items())), 'sampled_changes': dict(sorted(changes.items())),
            'sampled_states': dict(sorted(states.items())), 'dropped_total': group['dropped'],
            'missing_event_sequences': seqs[-1]-seqs[0]+1-len(seqs) if seqs else 0,
            'starts_mid_run': bool(seqs and seqs[0] > 1)})
    return report


def human_report(report):
    lines = [f"파일 {report['files']}개 / 실행 기록 {len(report['runs'])}개",
             f"잘못된 행 {report['invalid_lines']} / 중복 {report['duplicates']} / 충돌 {report['conflicts']}"]
    for run in report['runs']:
        lines.append(f"\n{run['session']} / {run['run']} ({run['evidence']})")
        counts = run['fallbacks'] if run['evidence'] == 'recorded_transitions' else run['sampled_changes']
        label = '기록된 1인칭 복귀' if run['evidence'] == 'recorded_transitions' else '표본 사이 상태 변화 (순간 전환은 누락 가능)'
        lines.append(label + ': ' + (', '.join(f'{k}={v}' for k, v in counts.items()) or '기록 없음'))
        for key, label in [('model_failures', '머리 제어 오류'), ('model_waits', '머리 대상 대기')]:
            if run[key]:
                lines.append(label + ': ' + ', '.join(f'{k}={v}' for k, v in run[key].items()))
        if run['dropped_total'] or run['missing_event_sequences'] or run['starts_mid_run']:
            lines.append(f"부분 기록: 큐 누락 {run['dropped_total']}, 중간 번호 공백 {run['missing_event_sequences']}, "
                         f"실행 중간부터 시작={run['starts_mid_run']}")
    lines.append('\n기록된 전환 조건을 집계합니다. 기둥·계단과의 인과관계나 실제 렌더링을 판정하지 않습니다.')
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='+', type=Path)
    parser.add_argument('--json', action='store_true', help='machine-readable report')
    args = parser.parse_args()
    try:
        report = summarize(args.paths)
    except ValueError as exc:
        print('Log summary failed: ' + str(exc), file=sys.stderr)
        return 2
    print(json.dumps(report, ensure_ascii=False, indent=2) if args.json else human_report(report))
    return 0


if __name__ == '__main__':
    sys.exit(main())
