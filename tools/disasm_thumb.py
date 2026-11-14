import sys,struct
sys.path.insert(0,'pylib')
from capstone import *
d=open(sys.argv[1],'rb').read(); base=0x08000000
start=int(sys.argv[2],16); end=int(sys.argv[3],16); mode=sys.argv[4] if len(sys.argv)>4 else 't'
md=Cs(CS_ARCH_ARM, CS_MODE_THUMB if mode=='t' else CS_MODE_ARM)
off=start-base
while off < end-base:
    got=False
    for ins in md.disasm(d[off:end-base], base+off):
        extra=''
        if 'pc' in ins.op_str and ins.mnemonic.startswith('ldr') and '#' in ins.op_str:
            try:
                imm=int(ins.op_str.split('#')[-1].rstrip(']'),16 if '0x' in ins.op_str.split('#')[-1] else 10)
                a=((ins.address+4)&~3)+imm
                extra='  ; =%#x'%struct.unpack_from('<I',d,a-base)[0]
            except Exception as e: pass
        print('%08x: %-8s %s%s'%(ins.address,ins.mnemonic,ins.op_str,extra))
        off=ins.address+ins.size-base; got=True
    if not got or off<end-base:
        print('%08x: .word %#x'%(base+off,struct.unpack_from('<H',d,off)[0])); off+=2
