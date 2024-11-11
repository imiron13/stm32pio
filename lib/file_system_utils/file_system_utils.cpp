#include <cstring>
#include <cstdlib>
#include <file_system_utils.h>
#include <shell.h>
#include <ff.h>

char current_folder[FF_MAX_LFN] = "/";

bool shell_cmd_ls(FILE *f, ShellCmd_t *cmd, const char *s)
{
    
    DIR dir;
    vTaskSuspendAll();
    int res = f_opendir(&dir, current_folder);
    xTaskResumeAll();
    if(res != FR_OK) {
       fprintf(f, "f_opendir() failed, res = %d" ENDL, res);
        return false;
    }

    FILINFO fileInfo;
    uint32_t totalFiles = 0;
    uint32_t totalDirs = 0;
    vTaskSuspendAll();
    for(;;) {
       res = f_readdir(&dir, &fileInfo);
       if((res != FR_OK) || (fileInfo.fname[0] == '\0')) {
           break;
       }

       if(fileInfo.fattrib & AM_DIR) {
           fprintf(f, "  DIR  %s" ENDL, fileInfo.fname);
           totalDirs++;
       } else {
           fprintf(f, "  FILE %s" ENDL, fileInfo.fname);
           totalFiles++;
       }
    }
    xTaskResumeAll();
    return true;
}

bool shell_cmd_pwd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, "%s" ENDL, current_folder);
    return true;
}

bool shell_cmd_cd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    bool is_success = true;
    DIR dir;
    const char *folder_arg = cmd->get_str_arg(s, 1);
    char *new_folder = (char*)malloc(FF_MAX_LFN);
    if (strcmp(folder_arg, "..") == 0)
    {
        int i = 0;
        int slash_pos = 0;
        while (current_folder[i] != 0)
        {
            if (current_folder[i] == '/')
            {
                slash_pos = i;
            }
            i++;
        }
        if (slash_pos == 0) slash_pos = 1;
        current_folder[slash_pos] = 0;
    }
    else
    {
        if (folder_arg[0] != '/')
        {
            strcpy(new_folder, current_folder);
            if (current_folder[strlen(current_folder)- 1] != '/')
            {
                strcat(new_folder, "/");
            }
        }

        strcat(new_folder, folder_arg);
        int res = f_opendir(&dir, new_folder);

        if(res != FR_OK) {
            fprintf(f, "Invalid path" ENDL);
            is_success = false;
        }
        else
        {
            strcpy(current_folder, new_folder);

        }
    }
    free(new_folder);
    return is_success;
}
