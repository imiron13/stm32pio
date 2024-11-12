#include <cstring>
#include <cstdlib>
#include <file_system_utils.h>
#include <shell.h>
#include <ff.h>

char FileSystemUtils_t::m_current_folder[FF_MAX_LFN] = "/";

bool FileSystemUtils_t::normalizePath(char *const path)
{
    char *src = nullptr;
    char *p = path;
    while (*p != 0)
    {
        if (*p == '/')
        {
            if (p[1] == '.' && p[2] == '.')
            {
                if (!src) return false;

                p += 3;
                while (*p != '/') 
                {
                    if (*p == 0) {
                        if (src > path + 1) src--;
                        *src = 0;
                        return true;
                    }
                    p++;
                }
                p++;

                while (*p != 0)
                {
                    *src++ = *p++;
                }
                *src = 0;
                src = nullptr;
                p = path;
                continue;
            }
            else
            {
                src = p + 1;
            }
        }
        p++;
    }

    return true;
}

bool FileSystemUtils_t::joinPath(char *const dst, const char *base, const char *ext)
{
    char *dst_ptr = dst;
    while (*base != 0) 
    {
        *dst_ptr++ = *base++;
    }

    if (*(dst_ptr - 1) != '/') 
    {
        *dst_ptr++ = '/';
        *dst_ptr = 0;
    }

    strcpy(dst_ptr, ext);
    return normalizePath(dst);
}

bool FileSystemUtils_t::relativePathToAbsolute(const char *rel_path, char *abs_path)
{
    if (rel_path[0] == '/')
    {
        strcpy(abs_path, rel_path);
        return true;
    }
    else
    {
        return joinPath(abs_path, m_current_folder, rel_path);
    }
}

const char *FileSystemUtils_t::getCurrentDir()
{
    return m_current_folder;
}

bool FileSystemUtils_t::changeDir(const char *dir_path)
{
    char *new_folder = (char*)malloc(FF_MAX_LFN);
    bool is_success = relativePathToAbsolute(dir_path, new_folder);
    if (is_success)
    {
        DIR dir;
        int res = f_opendir(&dir, new_folder);

        if(res != FR_OK) {
            is_success = false;
        }
        else
        {
            strcpy(m_current_folder, new_folder);
        }
    }
    free(new_folder);

    return is_success;
}

void FileSystemUtils_t::getFilesList()
{

}

bool FileSystemUtils_t::shell_cmd_ls(FILE *f, ShellCmd_t *cmd, const char *s)
{
    
    DIR dir;
    vTaskSuspendAll();
    int res = f_opendir(&dir, m_current_folder);
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

bool FileSystemUtils_t::shell_cmd_pwd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, "%s" ENDL, getCurrentDir());
    return true;
}

bool FileSystemUtils_t::shell_cmd_cd(FILE *f, ShellCmd_t *cmd, const char *s)
{
    const char *dir_path = cmd->get_str_arg(s, 1);
    bool is_ok = changeDir(dir_path);
    if (!is_ok)
    {
        fprintf(f, "Invalid path" ENDL);
        return false;
    }
    return true;
}
