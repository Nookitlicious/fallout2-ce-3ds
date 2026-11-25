int main(){
 char name[12]="Old"; answer="New Name 12"; pressed=SWKBD_BUTTON_RIGHT;
 assert(ctr_input_edit_text("Name",name,11,false,false)); assert(!strcmp(name,"New Name 12")); assert(limit==11&&!leftSubmit&&rightSubmit);
 answer="Discard"; pressed=SWKBD_BUTTON_LEFT; assert(!ctr_input_edit_text("Name",name,11,false,false)); assert(!strcmp(name,"New Name 12"));
 pressed=SWKBD_BUTTON_RIGHT; answer="Too long name"; assert(!ctr_input_edit_text("Name",name,11,false,false));
 answer=""; assert(!ctr_input_edit_text("Name",name,11,false,false));
 char save[30]="Original"; assert(ctr_input_edit_text("Save",save,29,true,false)&&save[0]==0);
 answer="Mixed Case 123!"; assert(ctr_input_edit_text("Save",save,29,true,false)&&!strcmp(save,answer));
 char file[9]="OLD"; answer="A/B"; assert(!ctr_input_edit_text("File",file,8,false,true)&&!strcmp(file,"OLD"));
 answer="A_123456"; assert(ctr_input_edit_text("File",file,8,false,true)&&!strcmp(file,answer));
 answer="\xc3\xa9"; assert(!ctr_input_edit_text("Save",save,29,true,false));
 const char* msg=nullptr; TextInputRules r={8,true}; assert(validateKeyboardText(&r,&msg,"A/B",3)==SWKBD_CALLBACK_CONTINUE); assert(validateKeyboardText(&r,&msg,"ABC",3)==SWKBD_CALLBACK_OK);
 puts("10.3 mocked native keyboard tests passed");
}
