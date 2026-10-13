"""Immutable read-only signatures from the user's original CT; never execute the CT."""
from __future__ import annotations
import argparse, hashlib, json, re, xml.etree.ElementTree as ET
from pathlib import Path
IDS = {"1337098144": "main", "1337098150": "camera_aim_collision", "1337098170": "auto_disable", "1337098210": "other", "1337098315": "fullbright"}
EXPECTED_HASH = "4154ee807842eb17f51b84272c98c3fdf153082c252f176446aa0cd61fd0686d"
def clean(script: str) -> str:
    script = re.sub(r"\{(?!\$).*?\}", lambda m: "\n" * m[0].count("\n"), script, flags=re.S)
    return re.sub(r"//[^\n]*", "", script)
def extract(raw: bytes) -> dict:
    sha = hashlib.sha256(raw).hexdigest()
    if sha != EXPECTED_HASH:
        raise ValueError("This extractor is pinned to the uploaded RT 8.02 CT hash; refuse other input")
    text = raw.decode("utf-8-sig")
    root = ET.fromstring(text)
    xml_scripts = list(re.finditer(r"<AssemblerScript>(.*?)</AssemblerScript>", text, re.S))
    entries = [e for e in root.iter("CheatEntry") if e.find("AssemblerScript") is not None]
    if len(entries) != len(xml_scripts): raise ValueError("XML/script count mismatch")
    patches, fixed = [], {}
    for entry, location in zip(entries, xml_scripts):
        eid = entry.findtext("ID")
        if eid not in IDS: continue
        start = text.count("\n", 0, location.start(1)) + 1
        script = clean(entry.findtext("AssemblerScript") or "")
        en, dis = script.split("[DISABLE]", 1)
        for m in re.finditer(r"(?im)^\s*aobscanmodule\((\w+),\s*re7\.exe,([^)]*)\)", en):
            name, pattern = m[1], " ".join(m[2].split())
            site = re.compile(r"(?im)^([ \t]*)(" + re.escape(name) + r"(?:[+-][0-9a-f]+)?):[ \t]*$")
            installs, restores = list(site.finditer(en)), list(site.finditer(dis))
            if len(installs) != 1 or len(restores) != 1: raise ValueError(f"ambiguous site {name}")
            ins, restore = installs[0], restores[0]
            expr = ins[2]
            if expr != restore[2]: raise ValueError(f"offset mismatch {name}")
            extra = expr[len(name):]
            delta = (1 if extra.startswith('+') else -1) * int(extra[1:],16) if extra else 0
            b = re.match(r"\s*db[ \t]+([0-9a-fA-F \t]+)(?:\n|$)",dis[restore.end():])
            if not b: raise ValueError(f"no fixed restore bytes {name}")
            expected = " ".join(b[1].upper().split())
            bytes.fromhex(expected)
            if any(len(v)!=2 for v in expected.split()): raise ValueError("invalid byte")
            patches.append(dict(name=name,group=IDS[eid],entry_id=eid,pattern=pattern,delta=delta,expected=expected,
                                ct_aob_line=start+en.count("\n",0,m.start())+m[0].count("\n"),
                                ct_site_line=start+en.count("\n",0,ins.start()),
                                ct_restore_line=start+script.count("\n",0,len(en)+len("[DISABLE]")+restore.start())))
        for i,line in enumerate(en.splitlines()):
            for m in re.finditer(r'(?:"re7\.exe"|re7\.exe)\+([0-9A-Fa-f]{6,})',line):
                kind = "branch" if re.match(r"\s*j\w+\s",line) else "slot"
                rva = int(m[1],16)
                key = (rva,kind)
                if key not in fixed: fixed[key]=dict(rva=rva,kind=kind,ct_line=start+i)
    if len(patches)!=78 or len({p['name'] for p in patches})!=78:raise ValueError("manifest site count changed")
    return dict(schema=1,source_name="RE7 Third-Person OTS RT 8.02",source_sha256=sha,
                scope="four main blocks (71) plus optional Fullbright (7); not all CT features",patches=patches,
                fixed_rvas=[fixed[k] for k in sorted(fixed)])
def header(m: dict) -> str:
    lines=["/* Generated from the pinned user-supplied CT. Read-only lookup data. */", '#include "core.h"',
           'typedef struct { const char *name, *group, *pattern, *expected; int delta; u32 ct_line; } PatchDef;',
           'typedef struct { u32 rva; const char *kind; u32 ct_line; } FixedDef;',
           '#define PATCH_COUNT 78', '#define CT_SHA256 "'+m['source_sha256']+'"', 'static const PatchDef PATCHES[PATCH_COUNT] = {']
    for p in m['patches']:
        lines.append('  {'+', '.join([json.dumps(p[k]) for k in ('name','group','pattern','expected')])+f", {p['delta']}, {p['ct_aob_line']}"+'},')
    lines += ['};',f"#define FIXED_COUNT {len(m['fixed_rvas'])}", 'static const FixedDef FIXED[FIXED_COUNT] = {']
    for f in m['fixed_rvas']: lines.append(f'  {{0x{f["rva"]:X}u, "{f["kind"]}", {f["ct_line"]}}},')
    return "\n".join(lines+['};',''])
if __name__ == "__main__":
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('ct',type=Path);ap.add_argument('--out',type=Path,default=Path(__file__).resolve().parents[1]);a=ap.parse_args()
    m=extract(a.ct.read_bytes());(a.out/'data').mkdir(parents=True,exist_ok=True);(a.out/'src').mkdir(parents=True,exist_ok=True)
    (a.out/'data/patch_manifest.json').write_text(json.dumps(m,indent=2)+"\n",encoding='utf8')
    (a.out/'src/manifest.h').write_text(header(m),encoding='utf8')
    print(f"generated {len(m['patches'])} sites, {len(m['fixed_rvas'])} fixed RVAs")
