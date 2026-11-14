#!/usr/bin/env python3
"""Compile the actual row declarations/mapping without the DSP's host ABI.

CPU2=0 host DSP builds currently fail core.h's track_t size assertion even on
our base revision. This harness tests both conditional menus without disabling
that production assertion or substituting the visibility implementation.
"""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
items=(root/'firmware/src/menu_items.c').read_text()
view=(root/'firmware/src/ui_menu.c').read_text()
rows=items[items.index('enum { MI_COLOR'):items.index('#define MI_VALUES')]
tabs=items[items.index('enum { MTAB_DISPLAY'):items.index('/* TUNE:')]
functions=view[view.index('static int menu_visible('):view.index('static int menu_large(')]
body=r'''
int main(void){assert(MTAB_NAME[0][0]=='D');
 unsigned counts[5]={3,6,6,6,6+FELUCCA_CPU2};
 for(unsigned t=0;t<5;t++){
  assert(menu_visible_count(t)==counts[t]);
  for(unsigned i=0;i<counts[t];i++){
   unsigned row=menu_visible_row(t,i);assert(row<MI_COUNT && MI_TAB[row]==t && menu_visible(row));
   assert(menu_visible_index(row)==i);
  }
  assert(menu_visible_row(t,counts[t])==MI_COUNT);
 }
 unsigned system[]={MI_SERIAL,MI_RESTORE,MI_HELP,
#if FELUCCA_CPU2
 MI_CPU2,
#endif
 MI_PANEL,MI_INFO,MI_ABOUT};
 for(unsigned i=0;i<sizeof system/sizeof *system;i++)assert(menu_visible_row(MTAB_SYSTEM,i)==system[i]);
 ui.menu_sel=MI_COLOR;menu_normalize();assert(ui.menu_sel==MI_ANIM);
 printf("conditional settings mapping: CPU2=%u, %u visible rows passed\n",FELUCCA_CPU2,27+FELUCCA_CPU2);
}
'''
with tempfile.TemporaryDirectory(prefix='coffee-settings-') as d:
 source=Path(d)/'mapping.c'
 source.write_text('#include <stdint.h>\n#include <assert.h>\n#include <stdio.h>\n'+rows+tabs+'\nstatic struct {uint8_t menu_sel,menu_row[MTAB_COUNT],force;} ui;\n'+functions+body)
 for cpu in (0,1):
  binary=Path(d)/f'mapping-{cpu}'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Werror',f'-DFELUCCA_CPU2={cpu}',str(source),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
