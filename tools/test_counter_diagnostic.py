#!/usr/bin/env python3
"""Synthetic integrity tests; fixtures are not performance evidence."""
import copy
import json
from pathlib import Path
import random
import unittest

from analyze_counter_diagnostic import (LABELS,PINS,SHARDS,campaign_seed,geometry,validate)
from generate_counter_diagnostic import generate


def fixture():
    metadata=[];records=[];source='a'*40
    for arch,compiler,capacity in sorted(SHARDS):
        seed=campaign_seed(arch,compiler,capacity);rng=random.Random(seed)
        metadata.append(dict(architecture=arch,compiler=compiler,capacity=capacity,source_commit=source,
            schema='veriqueue_cross_algorithm_campaign_v1',lane=f'{arch}-{compiler}',payloads=[8,16,64,256],
            implementations=list(LABELS),transfers=1000000,warmups=5,repetitions=50,seed=seed,
            ordering='randomized_per_round',program_sha256='a'*64))
        for pi,p in enumerate((8,16,64,256)):
            for ri in range(55):
                round_seed=rng.getrandbits(64);order=list(LABELS);random.Random(round_seed).shuffle(order)
                for oi,label in enumerate(order):
                    mode=1 if label.startswith('packed/') else 2 if label.startswith('split/') else 0
                    distance=256 if mode==2 else 8
                    records.append(dict(PINS,comparison_lane=f'{arch}-{compiler}',comparison_architecture=arch,
                        comparison_compiler=compiler,capacity=capacity,payload_bytes=p,topology='fixture',
                        producer_cpu=0,consumer_cpu=1,comparison_source_commit=source,
                        comparison_schema='veriqueue_cross_algorithm_campaign_v1',benchmark='baseline_compare_v2',
                        checksum=(1000000*999999//2*0x9e3779b185ebca87)%(1<<64),produced=1000000,
                        consumed=1000000,transfers=1000000,valid=True,affinity_valid=True,pinning_requested=True,
                        producer_pinned=True,consumer_pinned=True,implementation=label,warmup=ri<5,
                        campaign_seed=seed,transfers_per_second=1.0,repetition=ri if ri<5 else ri-5,
                        round_seed=round_seed,order_index=oi,campaign_index=(pi*55+ri)*15+oi,
                        observer_mode=mode,observer_geometry=[0,distance,768,769,772,distance,1,768,1]))
    return metadata,records,source


class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.meta,cls.records,cls.source=fixture()
    def test_full_matrix_and_schedule(self):
        self.assertEqual(len(validate(self.meta,self.records,self.source)),80)
    def test_missing_record(self):
        with self.assertRaises(ValueError):validate(self.meta,self.records[:-1],self.source)
    def test_wrong_source(self):
        with self.assertRaises(ValueError):validate(self.meta,self.records,'b'*40)
    def test_duplicate_with_missing_label(self):
        records=self.records[:];records[1]=records[0]
        with self.assertRaises(ValueError):validate(self.meta,records,self.source)
    def test_wrong_fifo(self):
        records=self.records[:];records[0]=dict(records[0],checksum=0)
        with self.assertRaises(ValueError):validate(self.meta,records,self.source)
    def test_counter_geometry(self):
        r=next(r for r in self.records if r['observer_mode']==2)
        for field,value in ((0,1),(1,255),(5,8),(6,0),(7,0),(8,2)):
            bad=copy.deepcopy(r);bad['observer_geometry'][field]=value
            with self.assertRaises(ValueError):geometry(bad)
        bad=copy.deepcopy(r);bad['observer_geometry'][0]=True
        with self.assertRaises(ValueError):geometry(bad)
    def test_generator_asserts_timed_work(self):
        base=Path('bench/bench_compare.cpp').read_text();out=generate(base)
        self.assertIn('consumed.store(expected, std::memory_order_relaxed);',out)
        self.assertIn('produced.store(i, std::memory_order_relaxed);',out)
        self.assertEqual(out.count('run_result run_pair_shared('),1)
        self.assertEqual(out.count('run_result run_pair_original('),1)
        self.assertEqual(out.count('observer_mode);'),5)


if __name__=='__main__':unittest.main()
