#pragma once
namespace fallout {
void ctrModProfileStartup();
void ctrModProfileStage(const char* stage);
void ctrModProfileStartupComplete();
void ctrModProfileLoadArchives();
const char* ctrModProfileArchive(const char* name);
const char* ctrModProfileConfigPath();
bool ctrModProfileActive();
bool ctrModProfileIsEtTu();
// 0 back, 1 preferences, 2 exit to restart.
int ctrModProfileMenu();
}
