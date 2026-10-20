"""Summarize a native-hook capture without claiming gameplay compatibility."""
import argparse,json,pathlib

def summarize(path):
 if path.stat().st_size>9*1024*1024:raise ValueError('Probe log exceeds expected bound')
 last=None;sites={};bad=0;headers=0;events=0
 with path.open(encoding='utf-8',errors='replace') as f:
  for line in f:
   try:row=json.loads(line)
   except (ValueError,TypeError):bad+=1;continue
   if not isinstance(row,dict):bad+=1;continue
   kind=row.get('type')
   if kind=='header':
    if row.get('schema')!=1 or row.get('mode')!='PASS_THROUGH_HOOK_PROBE':raise ValueError('Not a supported native probe log')
    headers+=1
   elif kind=='summary':last=row
   elif kind=='site_summary' and isinstance(row.get('site'),int) and row['site'] in range(5):sites[row['site']]=row
   elif kind=='hook':events+=1
 if headers!=1:raise ValueError('Expected one probe header')
 status=(last or {}).get('status')
 result='DEVICE_EVIDENCE_REQUIRES_REVIEW' if status=='CAPTURE_FINISHED_REVIEW_REQUIRED' else 'BLOCKED' if status in ('BLOCKED','HOOK_INSTALL_FAILED') else 'NOT_CAPTURED'
 return {'result':result,'reason':(last or {}).get('reason','MISSING_FINAL_SUMMARY'),
         'missing_sites':[i for i in range(5) if not sites.get(i,{}).get('calls',0)],
         'truncated_or_invalid_lines':bad,'logged_hook_events':events,
         'summary':last,'sites':[sites[i] for i in sorted(sites)],
         'limitations':'Counts/address overlap do not prove object lifetime, playable OTS, FPS, or same-frame camera/firing synchronization.'}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('log',type=pathlib.Path);args=parser.parse_args()
 try:print(json.dumps(summarize(args.log),indent=2,ensure_ascii=False))
 except (OSError,ValueError) as exc:parser.exit(2,str(exc)+'\n')
if __name__=='__main__':main()
