#!/usr/bin/env python3
"""Full80 observer-placement diagnostic, never an official queue promotion score."""
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
import random
import statistics

from analyze_comparator_campaign import (bootstrap_geomean_ci,bootstrap_median_ci,
    cell_key,classify,geometric_mean,load_records)
from analyze_policy_campaign import analyze as policy_analyze

QUEUES = ('veriqueue','rigtorp','boost_lockfree','moodycamel','drogalis')
LABELS = QUEUES + tuple('packed/'+q for q in QUEUES) + tuple('split/'+q for q in QUEUES)
PINS = dict(rigtorp_commit='59a6a938513ea5004817383711ed35d32385d3ee',
            moodycamel_commit='6867b56452352acf077fccd5f6cc7e3a8cfde0fb',
            drogalis_commit='c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96',boost_version=108300)
CAPACITIES=(2,64,256,1024,65536)
SHARDS={(a,c,n) for a in ('x64','arm64') for c in ('gcc','clang') for n in CAPACITIES}
SEED=2026101042


def campaign_seed(arch,compiler,capacity):
    return 2026101041+(100000 if arch=='arm64' else 0)+(10000 if compiler=='gcc' else 20000)+capacity


def context(label):
    if label in QUEUES: return 0,label
    prefix,queue=label.split('/',1)
    if prefix not in ('packed','split') or queue not in QUEUES: raise ValueError('Wrong label')
    return (1 if prefix=='packed' else 2),queue


def geometry(record):
    mode,_=context(record['implementation']);g=record.get('observer_geometry')
    if record.get('observer_mode')!=mode or not isinstance(g,list) or len(g)!=9 or any(type(v) is not int or v<0 for v in g):
        raise ValueError('Invalid observer geometry/mode')
    if any(v>=4096 for v in g[:5]) or g[6] not in (0,1) or g[8] not in (0,1): raise ValueError('Invalid residues/directions')
    if g[1]!=(g[0]+(g[5] if g[6] else -g[5]))%4096 or g[2]!=(g[0]+(g[7] if g[8] else -g[7]))%4096:
        raise ValueError('Inconsistent observer distances')
    if g[5]<8 or g[7]==0: raise ValueError('Overlapping counters/control')
    if mode and (g[0]%256 or g[5]!=(8 if mode==1 else 256) or g[6]!=1): raise ValueError('Observer layout intervention failed')
    if mode and g[8]==1 and g[7]<512: raise ValueError('Failed flag overlaps progress storage')
    return mode


def validate(metadata,records,source):
    if len(metadata)!=20 or len(records)!=66000: raise ValueError('Require 20 shards and 66,000 records')
    metas={};groups=defaultdict(list);pairs=defaultdict(dict)
    checksum=(1000000*999999//2*0x9e3779b185ebca87)%(1<<64)
    for m in metadata:
        shard=m['architecture'],m['compiler'],m['capacity']
        if shard not in SHARDS or shard in metas:raise ValueError('Wrong/duplicate shard')
        if (m['source_commit']!=source or m['schema']!='veriqueue_cross_algorithm_campaign_v1' or
            m['lane']!=f'{shard[0]}-{shard[1]}' or m['payloads']!=[8,16,64,256] or
            m['implementations']!=list(LABELS) or m['transfers']!=1000000 or m['warmups']!=5 or
            m['repetitions']!=50 or m['ordering']!='randomized_per_round' or m['seed']!=campaign_seed(*shard) or
            len(m['program_sha256'])!=64 or any(c not in '0123456789abcdef' for c in m['program_sha256'])):
            raise ValueError('Wrong source/protocol')
        metas[shard]=m
    if set(metas)!=SHARDS:raise ValueError('Incomplete matrix')
    for r in records:
        key=cell_key(r);shard=key[1],key[2],key[3]
        if (shard not in metas or key[0]!=f'{key[1]}-{key[2]}' or key[4] not in (8,16,64,256) or
            key[6]<0 or key[7]<0 or key[6]==key[7] or r['comparison_source_commit']!=source or
            r['comparison_schema']!='veriqueue_cross_algorithm_campaign_v1' or
            r.get('benchmark')!='baseline_compare_v2' or r.get('checksum')!=checksum or
            any(r.get(k)!=v for k,v in PINS.items()) or
            any(r.get(k)!=1000000 for k in ('produced','consumed','transfers')) or
            any(r.get(k) is not True for k in ('valid','affinity_valid','pinning_requested','producer_pinned','consumer_pinned')) or
            r['implementation'] not in LABELS or type(r['warmup']) is not bool or
            r['campaign_seed']!=campaign_seed(*shard) or
            not math.isfinite(float(r['transfers_per_second'])) or r['transfers_per_second']<=0):
            raise ValueError('Invalid source/workload/FIFO/pins/affinity/rate')
        geometry(r)
        if r['repetition'] not in range(5 if r['warmup'] else 50):raise ValueError('Wrong repetition')
        groups[key,r['warmup'],r['repetition']].append(r)
    for shard,m in sorted(metas.items()):
        rng=random.Random(campaign_seed(*shard))
        for pi,payload in enumerate((8,16,64,256)):
            for ri in range(55):
                warmup=ri<5;rep=ri if warmup else ri-5
                matches=[(k,rs) for (k,w,n),rs in groups.items() if (k[1],k[2],k[3])==shard and k[4]==payload and w==warmup and n==rep]
                if len(matches)!=1:raise ValueError('Fragmented round context')
                key,rs=matches[0];seed=rng.getrandbits(64);order=list(LABELS);random.Random(seed).shuffle(order)
                indexed=sorted(rs,key=lambda r:r['campaign_index'])
                if len(rs)!=15 or [r['implementation'] for r in indexed]!=order:raise ValueError('Incomplete/duplicate/order-invalid round')
                for oi,r in enumerate(indexed):
                    if r['round_seed']!=seed or r['order_index']!=oi or r['campaign_index']!=(pi*55+ri)*15+oi:
                        raise ValueError('Randomized schedule/chronology mismatch')
                if not warmup:pairs[key][rep]={r['implementation']:r['transfers_per_second'] for r in rs}
    if len(pairs)!=80 or len(groups)!=4400:raise ValueError('Wrong cells/rounds')
    return pairs


def comparison(pairs,queue,numerator,denominator,seed):
    rows=[]
    for i,(key,reps) in enumerate(sorted(pairs.items())):
        ratios=[r[numerator]/r[denominator] for _,r in sorted(reps.items())]
        low,high=bootstrap_median_ci(ratios,seed^(i*0x9E3779B1),20000)
        rows.append(dict(lane=key[0],capacity=key[3],payload=key[4],queue=queue,n=50,
                         ratio=statistics.median(ratios),low=low,high=high,classification=classify(low,high)))
    summaries=[]
    for i,lane in enumerate(['all']+sorted({r['lane'] for r in rows})):
        selected=[r for r in rows if lane=='all' or r['lane']==lane];values=[r['ratio'] for r in selected]
        low,high=bootstrap_geomean_ci(values,seed^i,20000)
        summaries.append(dict(lane=lane,ratio=geometric_mean(values),low=low,high=high,
                             classification=classify(low,high),counts=dict(Counter(r['classification'] for r in selected))))
    return dict(queue=queue,numerator=numerator,denominator=denominator,bootstrap_seed=seed,cells=rows,summaries=summaries)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('input',type=Path)
    parser.add_argument('output',type=Path);parser.add_argument('--source-commit',required=True)
    args=parser.parse_args();metadata,records=load_records(sorted(args.input.glob('*.jsonl')))
    pairs=validate(metadata,records,args.source_commit);primary=[];calibration=[]
    for i,q in enumerate(QUEUES):
        primary.append(comparison(pairs,q,'split/'+q,'packed/'+q,SEED^(i*0x85EBCA77)))
        calibration.append(comparison(pairs,q,'packed/'+q,q,SEED^0x517CC1B7^(i*0x85EBCA77)))
    policies={}
    for mode,prefix in enumerate(('','packed/','split/')):
        selected=[dict(r,implementation=context(r['implementation'])[1]) for r in records if context(r['implementation'])[0]==mode]
        policies[('original','packed','split')[mode]]=policy_analyze(selected,('veriqueue',),QUEUES[1:],SEED^mode,20000)
    hist=[]
    for lane in sorted({r['comparison_lane'] for r in records}):
        for label in LABELS:
            chosen=[r for r in records if r['comparison_lane']==lane and r['implementation']==label and not r['warmup']]
            values=Counter(tuple(r['observer_geometry']) for r in chosen)
            hist.append(dict(lane=lane,implementation=label,n=len(chosen),
                             geometries=[dict(values=list(k),count=v) for k,v in sorted(values.items())]))
    result=dict(schema='veriqueue_counter_placement_diagnostic_v1',source_commit=args.source_commit,
                primary=primary,calibration=calibration,policy_by_harness=policies,observer_geometry=hist,
                integrity=dict(shards=20,cells=80,records=66000,warmup_records=6000,measured_records=60000,
                               complete_rounds=4400,transfers=1000000,pins=PINS,schedule_geometry_verified=True),
                interpretation='Harness diagnosis only. Original80 official score unchanged; TIE is not equivalence.')
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({x['queue']:x['summaries'] for x in primary},indent=2))
    return 0


if __name__=='__main__':raise SystemExit(main())
