#!/usr/bin/env python3
"""Read-only research census; writes disc-derived evidence only to an ignored directory.

Run from native-port: python3 tools/research_after_11a.py
No Wine, Ghidra, installed dependencies, or game-source mutation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def u16(b, p):
    return struct.unpack_from('<H', b, p)[0]


def u32(b, p):
    return struct.unpack_from('<I', b, p)[0]


def resources(b):
    assert b[:4] == b'CRSR'
    t = u32(b, 16)
    assert b[t:t+4] == b'LBTR'
    result = {}
    for i in range(u32(b, t+8)):
        p = t+16+32*i
        offset, size = u32(b, p+8), u32(b, p+12)
        assert offset+size <= len(b)
        result[(b[p:p+4][::-1].decode('ascii'), u32(b, p+4))] = b[offset:offset+size]
    return result


def riff(b, first=0, end=None, prefix=''):
    end = len(b) if end is None else end
    result = {}
    while first < end:
        assert first+8 <= end
        tag = b[first:first+4].decode('ascii')
        size = u32(b, first+4)
        last = first+8+size
        assert last <= end
        if tag in ('RIFF', 'LIST'):
            assert size >= 4
            kind = b[first+8:first+12].decode('ascii')
            result.update(riff(b, first+12, last, prefix+kind+'/'))
        else:
            result[prefix+tag] = b[first+8:last]
        first = last+(size & 1)
    return result


def midi(b):
    if b[:4] == b'RIFF':
        return mids(b)
    assert b[:4] == b'MThd' and u32be(b, 4) == 6
    fmt, tracks, division = struct.unpack_from('>HHH', b, 8)
    p = 14
    channel = {}
    tempos = []
    sysex = []
    def vlq(q, end):
        value = 0
        for _ in range(4):
            assert q < end
            v = b[q]; q += 1
            value = (value << 7) | (v & 127)
            if not v & 128:
                return value, q
        raise ValueError('oversized MIDI VLQ')
    for track in range(tracks):
        assert b[p:p+4] == b'MTrk'
        end = p+8+u32be(b, p+4); p += 8
        assert end <= len(b)
        tick = 0; running = None
        while p < end:
            delta, p = vlq(p, end); tick += delta
            status = b[p]
            if status & 128:
                p += 1
                running = status if status < 240 else None
            else:
                assert running is not None
                status = running
            if status == 255:
                kind = b[p]; p += 1
                length, p = vlq(p, end)
                assert p+length <= end
                if kind == 81:
                    tempos.append([track, tick, int.from_bytes(b[p:p+length], 'big')])
                p += length
            elif status in (240, 247):
                length, p = vlq(p, end)
                assert p+length <= end
                sysex.append([track, tick, b[p:p+length].hex()]); p += length
            else:
                assert 128 <= status < 240
                count = 1 if status >> 4 in (12, 13) else 2
                assert p+count <= end and all(x < 128 for x in b[p:p+count])
                values = list(b[p:p+count]); p += count
                c = channel.setdefault(status & 15, {'programs': set(), 'controllers': {}, 'notes': set(), 'events': 0})
                c['events'] += 1
                if status >> 4 == 12:
                    c['programs'].add(values[0])
                if status >> 4 == 11:
                    c['controllers'].setdefault(values[0], set()).add(values[1])
                if status >> 4 == 9 and values[1]:
                    c['notes'].add(values[0])
        assert p == end
    assert p == len(b)
    for c in channel.values():
        c['programs'] = sorted(c['programs']); c['notes'] = sorted(c['notes'])
        c['controllers'] = {k: sorted(v) for k, v in c['controllers'].items()}
    return {'format': fmt, 'tracks': tracks, 'division': division, 'channels_0_based': channel,
            'tempos': tempos, 'sysex': sysex}


def u32be(b, p):
    return struct.unpack_from('>I', b, p)[0]


def mids(b):
    chunks = riff(b)
    division, max_buffer, flags = struct.unpack('<III', chunks['MIDS/fmt '])
    data = chunks['MIDS/data']; p = 4; tick = 0
    channels = {}; tempos = []; long_events = []; blocks = []; event_types = {}; programs = {}
    for i in range(u32(data,0)):
        timestamp, size = u32(data,p), u32(data,p+4); p += 8
        end = p+size
        assert size <= max_buffer and end <= len(data)
        blocks.append({'start_tick':timestamp,'bytes':size,'accumulated_tick':tick})
        while p < end:
            stride = 8 if flags & 1 else 12
            assert p+stride <= end
            delta = u32(data,p); event = u32(data,p+stride-4); p += stride
            tick += delta
            kind = (event >> 24) & 191
            event_types[kind] = event_types.get(kind,0)+1
            if event & 0x80000000:
                length = event & 0xffffff
                assert p+((length+3)&~3) <= end
                long_events.append({'tick':tick,'type':kind,'bytes':length,'hex':data[p:p+length].hex()})
                p += (length+3)&~3
            elif kind == 1:
                tempos.append([tick,event & 0xffffff])
            elif kind == 0:
                status,a,v = event&255,(event>>8)&255,(event>>16)&255
                assert 128 <= status < 240 and a < 128 and v < 128
                c = channels.setdefault(status&15,{'programs':set(),'controllers':{},'notes':set(),'events':0,'sounding_programs':set()})
                c['events'] += 1
                if status>>4==12:
                    c['programs'].add(a); programs[status&15]=a
                if status>>4==11: c['controllers'].setdefault(a,set()).add(v)
                if status>>4==9 and v:
                    c['notes'].add(a); c['sounding_programs'].add(programs.get(status&15,0))
        assert p == end
    assert p == len(data)
    for c in channels.values():
        c['programs']=sorted(c['programs']); c['notes']=sorted(c['notes'])
        c['sounding_programs']=sorted(c['sounding_programs'])
        c['controllers']={k:sorted(v) for k,v in c['controllers'].items()}
    return {'format':'RIFF MIDS','division':division,'max_buffer':max_buffer,'flags':flags,
            'ticks':tick,'blocks':blocks,'channels_0_based':channels,'tempos':tempos,
            'long_events':long_events,'event_types':event_types}


def census(root):
    exe = (root/'ROADRASH.EXE').read_bytes()
    digest = hashlib.sha256(exe).hexdigest()
    assert digest == '73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2'
    pe = u32(exe, 60)
    count = u16(exe, pe+6); opt_size = u16(exe, pe+20)
    base = u32(exe, pe+24+28)
    sections = []
    for i in range(count):
        p = pe+24+opt_size+40*i
        sections.append((base+u32(exe,p+12), u32(exe,p+16), u32(exe,p+20)))
    def at(va, length):
        for start, size, raw in sections:
            if start <= va and va+length <= start+size:
                return exe[raw+va-start:raw+va-start+length]
        raise ValueError(hex(va))
    def words(va, n):
        return list(struct.unpack('<'+'I'*n, at(va,n*4)))
    def string(va):
        return at(va,64).split(b'\0')[0].decode('ascii')
    tables = {'menu_mileage_tenths': words(0x468f58,25),
              'music_generic': [string(p) for p in words(0x46d908,6)],
              'music_awe': [string(p) for p in words(0x46d920,6)],
              'banks_melodic': [string(p) for p in words(0x46d938,6)],
              'controls_pairs': list(at(0x465fb8,22)),
              'control_bits': words(0x476c00,22),
              'hazard_template': words(0x476cb8,7),
              'horizons': [string(p) for p in words(0x467aa0,5)],
              'jukebox_videos': [string(p) for p in words(0x473f98,2)],
              'race_window_message_dispatch': words(0x423f80,7),
              'race_keyup_dispatch': {hex(key):hex(words(0x423f9c+4*at(0x423fd8+key-0x10,1)[0],1)[0])
                                      for key in range(0x10,0x7a)}}
    depth = words(0x46f098,4352)
    depth_errors = [abs(x-min(163840,16777216//(2*i))) for i,x in enumerate(depth) if i]
    tables['depth_reciprocal_check'] = {'entries':len(depth),'mismatches':sum(e!=0 for e in depth_errors),
        'max_error':max(depth_errors),'last':depth[-1]}
    courses = {}
    for name in ('CITY','HIWAY','MEDLY','NAPA','CANYN'):
        r = resources((root/'DATA/COURSES'/f'{name}.CRS').read_bytes())
        s, graph = r[('SGS ',1)], r[('NOD ',1)]
        sections_out = []
        for i in range(u32(s,8)):
            row = 16+52*i
            events, families, objects = u32(s,row+36), u32(s,row+12), u32(s,row+28)
            cesr = [] if not events else [
                {'start': u32(s,events+20+12*k), 'end': u32(s,events+24+12*k),
                 'raw_type': s[events+28+12*k], 'parameters': list(s[events+29+12*k:events+32+12*k])}
                for k in range(u32(s,events+8))]
            terrain = u32(s,row+16)
            sections_out.append({'section':i,'samples':u32(s,u32(s,row+4)+8),
                'terrain_tag':s[terrain:terrain+4].decode('ascii') if terrain else None,
                'events':cesr,'families':[] if not families else sorted({u32(s,families+32+12*k) for k in range(u32(s,families+8))}),
                'dzhr_count':0 if not objects else u32(s,objects+8)})
        pending = [u32(graph,16)]; nodes = {}
        while pending:
            p = pending.pop()
            if p in nodes: continue
            kind = u32(graph,p)
            node = {'kind':kind}
            if kind < 3:
                node.update(next=u32(graph,p+8),previous=u32(graph,p+12),link=u32(graph,p+16),length=u32(graph,p+20))
                pending.extend([node['next'],node['previous']])
                if kind: pending.append(node['link'])
            nodes[p] = node
        assert len(nodes) == u32(graph,8)
        # Independent choice at every split; a global all-left/all-right toggle misses mixed routes.
        routes = []
        def walk(p, sample, choices, seen, finishes):
            assert p not in seen, 'cycle in forward course graph'
            node = nodes[p]; seen = seen | {p}
            if node['kind'] == 0:
                ev = sections_out[node['link']]['events']
                finishes = finishes + [dict(e,section=node['link'],node=p,position_q8=(sample+e['start'])*256)
                                       for e in ev if e['raw_type']==6 and e['start'] < node['length']]
                walk(node['next'], sample+node['length'], choices, seen, finishes)
            elif node['kind'] == 1:
                for branch,target in enumerate([node['next'],node['previous']]):
                    walk(target,sample,choices+[[p,branch]],seen,finishes)
            elif node['kind'] == 2:
                walk(node['link'],sample,choices,seen,finishes)
            else:
                routes.append({'choices':choices,'samples':sample,'terminal_kind':node['kind'],'finish_events':finishes})
        walk(u32(graph,16),0,[],set(),[])
        cars = resources((root/'DATA/CARS'/f'{name}.CAR').read_bytes())
        courses[name]={'sections':sections_out,'nodes':nodes,'routes':routes,
                       'car_resources':[{'tag':tag,'id':id,'size':len(data)} for (tag,id),data in cars.items()]}
    music = {}
    for p in sorted((root/'AUDIO/MUSIC').glob('*.SBK')):
        chunks = riff(p.read_bytes())
        phdr, inst = chunks['sfbk/pdta/phdr'], chunks['sfbk/pdta/inst']
        music[p.name]={'chunks':{k:len(v) for k,v in chunks.items()},
            'version':list(struct.unpack('<HH',chunks['sfbk/INFO/ifil'])),
            'presets':[{'program':u16(phdr,q+20),'bank':u16(phdr,q+22),'bag':u16(phdr,q+24)} for q in range(0,len(phdr)-38,38)],
            'instrument_count':len(inst)//22-1,
            'igen_ops':sorted({u16(chunks['sfbk/pdta/igen'],q) for q in range(0,len(chunks['sfbk/pdta/igen'])-4,4)}),
            'sample_headers_u32':[list(struct.unpack('<IIII',chunks['sfbk/pdta/shdr'][q:q+16])) for q in range(0,len(chunks['sfbk/pdta/shdr']),16)]}
    for p in sorted((root/'AUDIO/MUSIC').glob('*.MID')):
        music[p.name]=midi(p.read_bytes())
    videos=sorted(str(p.relative_to(root)) for p in (root/'VIDEO').rglob('*.AVI'))
    return {'exe_sha256':digest,'tables':tables,'courses':courses,'music':music,'video_files':videos}


def self_check():
    def chunk(tag, body):
        return tag+struct.pack('<I',len(body))+body+(b'\0' if len(body)&1 else b'')
    def fixture(flags, blocks):
        data=struct.pack('<I',len(blocks))+b''.join(struct.pack('<II',t,len(b))+b for t,b in blocks)
        return chunk(b'RIFF',b'MIDS'+chunk(b'fmt ',struct.pack('<III',480,4096,flags))+chunk(b'data',data))
    first=struct.pack('<III',0,0xffffffff,0x0107a120)+struct.pack('<III',7,0,0x00643c90)
    second=struct.pack('<III',3,0,0x00003c80)
    normal=fixture(0,[(0,first),(7,second)])
    parsed=midi(normal)
    assert parsed['ticks']==10 and parsed['tempos']==[[0,500000]]
    assert parsed['channels_0_based'][0]['notes']==[60]
    compact=fixture(1,[(0,struct.pack('<II',0,0x0107a120)+struct.pack('<II',9,0x00643c90))])
    assert midi(compact)['ticks']==9
    long_event=fixture(0,[(0,struct.pack('<III',2,0,0x80000003)+b'\xf0\x01\xf7\0')])
    assert midi(long_event)['long_events'][0]['hex']=='f001f7'
    for case in (normal,compact,long_event):
        for size in range(len(case)):
            try:
                midi(case[:size])
            except (AssertionError,KeyError,IndexError,struct.error,UnicodeDecodeError,ValueError):
                continue
            raise AssertionError(f'Accepted truncated synthetic MIDS at {size}')
    print('Synthetic MIDS: multi-block, compact, padded long event; 232 truncations rejected: PASS')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets',type=Path,default=Path('../cdrom/ROADRASH'))
    parser.add_argument('--out',type=Path,default=Path('analysis/oracle/post11a-research/census.json'))
    parser.add_argument('--self-check',action='store_true',help='Check synthetic MIDS cases before the disc census')
    args=parser.parse_args()
    if args.self_check:
        self_check()
    # Avoid accidentally committing disc-derived resource tables.
    assert 'analysis' in args.out.parts and 'oracle' in args.out.parts
    result=census(args.assets)
    disasm=Path('analysis/original-disassembly.txt').read_text()
    result['crash_call_sites']=[line.strip() for line in disasm.splitlines() if re.search(r'calll\s+0x403d60\b',line)]
    result['selected_disassembly']={}
    ranges = [(0x4525c0,0x452610),(0x45443e,0x45445d),(0x4052c0,0x40544a),
              (0x40fbf0,0x40fc60),(0x446c30,0x447b19),(0x4232a0,0x423798)]
    for first,last in ranges:
        lines=[]
        for line in disasm.splitlines():
            match=re.match(r'\s*([0-9a-f]+):',line)
            if match and first <= int(match[1],16) < last: lines.append(line)
        result['selected_disassembly'][hex(first)] = lines
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(f"Verified reference executable; wrote {args.out}")
    for name,c in result['courses'].items():
        print(name,'sections',len(c['sections']),'routes',len(c['routes']),
              'samples',sorted({r['samples'] for r in c['routes']}))
    print('Music:',len(result['music']),'files;',len(result['crash_call_sites']),'direct crash call sites')


if __name__=='__main__':
    main()
