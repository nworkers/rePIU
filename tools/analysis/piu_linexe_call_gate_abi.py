#!/usr/bin/env python3
"""Recover PIU's LINEXE export slots and observed wrapper ABI."""
import argparse, json, struct
from pathlib import Path

EXPORTS = (
 (0x962C0,"LINEXE_LOADMODULE",0xE3440,["module_name_far"]),
 (0x962C8,"LINEXE_FREEMODULE",0xE34B0,["module_handle"]),
 (0x962D0,"GETLOADTABLE",0xE34E8,["load_table_far"]),
 (0x962D8,"GETLOADNAME",0xE356C,["module_handle","name_buffer_far","buffer_size"]),
 (0x962E0,"LINEXE_GETMODHANDLE",0xE35C8,["module_name_far","module_handle_far"]),
 (0x962E8,"LINEXE_GETPROCADDR",0xE3648,["procedure_name","module_handle_far","procedure_far"]),
 (0x962F0,"REL",0xE36F4,["region_far","byte_count"]),
 (0x962F8,"UNREL",0xE3754,["region_far","byte_count"]),
)
def u32(d,o): return struct.unpack_from("<I",d,o)[0]
def map_object(d,le,index):
 ot,pt,dp,ps=u32(d,le+0x40),u32(d,le+0x48),u32(d,le+0x80),u32(d,le+0x28)
 size,base,_,first,count,_=struct.unpack_from("<6I",d,le+ot+(index-1)*24)
 image=bytearray(size)
 for j in range(count):
  e=le+pt+(first-1+j)*4; page=int.from_bytes(d[e:e+3],"big"); n=min(ps,size-j*ps)
  if page: image[j*ps:j*ps+n]=d[dp+(page-1)*ps:dp+(page-1)*ps+n]
 return base,image
def cstr(d,o):
 end=d.find(b"\0",o)
 if end<0: raise ValueError(f"unterminated string at 0x{o:X}")
 return d[o:end].decode("ascii")
def main():
 p=argparse.ArgumentParser(); p.add_argument("exe",type=Path); p.add_argument("--output",type=Path); a=p.parse_args()
 d=a.exe.read_bytes(); le=u32(d,0x3c)
 if d[le:le+2]!=b"LE": raise SystemExit("not an LE executable")
 cb,code=map_object(d,le,2); db,data=map_object(d,le,4)
 if (cb,db)!=(0x20000,0x120000): raise SystemExit("unexpected PIU object layout")
 found=[]
 for slot,name,wrapper,args in EXPORTS:
  actual=cstr(data,u32(data,slot))
  if actual!=name or code[wrapper]!=0x55: raise SystemExit(f"ABI evidence mismatch at {name}")
  found.append({"slot_offset":f"0x{slot:08X}","resolved_pointer_offset":f"0x{slot+4:08X}","name":name,"wrapper_object2_offset":f"0x{wrapper:08X}","arguments_in_push_order":args})
 report={"input":str(a.exe),"bridge":{"target":"EDI","selector":"BX","return_object2_offset":"0x000E37B2"},"exports":found}
 text=json.dumps(report,indent=2)+"\n"
 if a.output: a.output.write_text(text,encoding="utf-8")
 else: print(text,end="")
if __name__=="__main__": main()
