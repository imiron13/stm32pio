#include "main.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <malloc.h>

#include <gpio_pin_stm32.h>
#include <board.h>
#include <hal_wrapper_stm32.h>
#include <uart_stdio_stm32.h>

#include <gpio_pin.h>
#include <led.h>
#include <button.h>
#include <shell.h>
#include <file_system_utils.h>
#include <vt100_terminal.h>
#include <tetris.h>
#include <usb_vcom_stdio_stm32.h>
#include <audio_volume_control.h>
#include <wav_player.h>
#include <audio_player.h>
#include <audio_player_shell_ui.h>

#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include "usb_device.h"

#include <epaper_weact.h>
#include <FreeMono12pt7b.h>

using namespace std;

#ifndef BOARD_SUPPORT_SD_CARD
#define BOARD_SUPPORT_SD_CARD                                   (0)
#endif

#ifndef BOARD_SUPPORT_AUDIO
#define BOARD_SUPPORT_AUDIO                                     (0)
#endif

#define EN_SD_CARD                                          (1 && (BOARD_SUPPORT_SD_CARD == 1))
#define EN_FATFS                                            (1 && (EN_SD_CARD == 1))

#define EN_SD_CARD_READ_WRITE_SHELL_CMDS                    (1 && (EN_SD_CARD == 1))
#define EN_FATFS_SHELL_CMDS                                 (1 && (EN_FATFS == 1))
#define EN_AUDIO_SHELL_CMDS                                 (1 && (BOARD_SUPPORT_AUDIO == 1))
#define EN_TETRIS                                           (1)

#if (EN_FATFS == 1)
#include "ff.h"
FATFS fs;
#endif

#if (BOARD_SUPPORT_AUDIO == 1)
#include "i2s.h"
int g_volume_shift = 0;
#endif

static const uint32_t SD_CARD_BLOCK_SIZE_IN_BYTES = 512;  

extern UART_HandleTypeDef huart1;
#if (EN_SD_CARD == 1)
extern SD_HandleTypeDef hsd;
#endif

Shell_t shell;

bool shell_cmd_clear_screen(FILE *f, ShellCmd_t *cmd, const char *s)
{
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME);
    return true;
}

#if (EN_TETRIS == 1)
TetrisGame_t<16, 16> tetris;

bool shell_cmd_tetris(FILE *f, ShellCmd_t *cmd, const char *s)
{
    tetris.set_device(f);
    tetris.start_game();

    bool quit = false;
    while (!quit)
    {
        quit = tetris.run_ui();
        HAL_Delay(tetris.get_ui_update_period_ms());
    }
    fprintf(f, BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    fprintf(f, "Thanks for playing!" ENDL);
    return true;
}
#endif  

bool shell_cmd_led(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int state = cmd->get_int_arg(s, 1);
    if (state == 0)
    {
        led1.off();
    }
    else
    {
        led1.on();
    }
    return true;
}

#if (EN_SD_CARD_READ_WRITE_SHELL_CMDS == 1)
uint8_t sd_card_buf[SD_CARD_BLOCK_SIZE_IN_BYTES];

bool shell_cmd_sd_card_read(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int blk = cmd->get_int_arg(s, 1);
    int num_bytes = cmd->get_int_arg(s, 2);
    HAL_StatusTypeDef res = HAL_SD_ReadBlocks(&hsd, (uint8_t *)sd_card_buf, blk, 1, HAL_MAX_DELAY);
    //int res = SDCARD_ReadSingleBlock(blk, sd_card_buf);
    if (res != HAL_OK)
     {
        fprintf(f, "Error reading SD card block %u, error = %d" ENDL, blk, (int)hsd.ErrorCode);
        return false;
    }
    else
    {
        for (int i = 0; i < num_bytes; i++)
        {
            fprintf(f, "%02X ", sd_card_buf[i]);
            if ((i % 8) == 7)
            {
                fprintf(f, ENDL);
            }
        }
        fprintf(f, ENDL);
        return true;
    }
}

bool shell_cmd_sd_card_write(FILE *f, ShellCmd_t *cmd, const char *s)
{
    int blk = cmd->get_int_arg(s, 1);
    HAL_StatusTypeDef status = HAL_SD_WriteBlocks(&hsd, sd_card_buf, blk, 1, HAL_MAX_DELAY);

    if (status != HAL_OK)
    {
        fprintf(f, "Error writing SD card block %d, error = %d" ENDL, blk, (int)hsd.ErrorCode);
        return false;
    }
    else
    {
        fprintf(f, "Successfully written to SD card block %d" ENDL, blk);
        return true;
    }
}
#endif

#if (EN_FATFS_SHELL_CMDS == 1)
#endif

#if (EN_AUDIO_SHELL_CMDS == 1)
#include <math.h>

#define PI 3.14159265358979323846
#define TAU (2.0 * PI)

bool shell_cmd_test_dac(FILE *f, ShellCmd_t *cmd, const char *s) {
    HAL_StatusTypeDef res;
    int nsamples = 4096;
    int16_t *dac_signal = (int16_t*)malloc(nsamples * sizeof(int16_t));
    if (dac_signal == NULL)
    {
        fprintf(f, "Heap alloc error" ENDL);
        return false;
    }

    int i = 0;
    while(i < nsamples) {
        double t = ((double)i/2.0)/((double)nsamples);
        dac_signal[i] = 1023*sin(100.0 * TAU * t); // left
        dac_signal[i+1] = dac_signal[i]; // right
        i += 2;
    }  

    bool end = false;
    while (!end) {
        res = HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)dac_signal, nsamples);
        /*res = HAL_I2S_Transmit(&hi2s2, (uint16_t*)dac_signal, nsamples,
                               HAL_MAX_DELAY);*/
        /*if(res != HAL_OK) {
            fprintf(f, "I2S - ERROR, res = %d!\r\n", res);
            return false;
        }*/

        int c = fgetc(f);
        if (c =='q')
        {
            //HAL_I2S_DMAStop();
            end = 1;
        }
    }
    free(dac_signal);
    return (res == HAL_OK);
} 

#define DAC_BUF_NUM_SAMPLES     (4096)
#define DAC_BUF_SIZE            (DAC_BUF_NUM_SAMPLES * sizeof(int16_t))

#endif

#if (EN_FATFS == 1)
bool init_fatfs()
{
    printf("FATFS init...");
    FRESULT res;

    // mount the default drive
    res = f_mount(&fs, "", 0);
    if(res != FR_OK) {
        printf("f_mount() failed, res = %d" ENDL, res);
        return false;
    }

    uint32_t freeClust;
    FATFS* fs_ptr = &fs;
    // Warning! This fills fs.n_fatent and fs.csize!
    res = f_getfree("", &freeClust, &fs_ptr);
    if(res != FR_OK) {
        printf("f_getfree() failed, res = %d" ENDL, res);
        return false;
    }

    uint32_t totalBlocks = (fs.n_fatent - 2) * fs.csize;
    uint32_t freeBlocks = freeClust * fs.csize;
    printf("done" ENDL);
    printf("Total blocks: %lu (%lu MiB), free blocks: %lu (%lu MiB), cluster=%lu B" ENDL,
                totalBlocks, totalBlocks / 2048,
                freeBlocks, freeBlocks / 2048,
                fs.csize * SD_CARD_BLOCK_SIZE_IN_BYTES);
    return true;
}   
#endif

bool init_storage()
{
#if (EN_SD_CARD == 1)    
#if (EN_FATFS == 1)
    return init_fatfs();
#else
    printf("SD card init...");
    HAL_SD_CardInfoTypeDef card_info;
    HAL_SD_GetCardInfo(&hsd, &card_info);

    printf("done, num_blks=%lu, blk_size=%lu" ENDL, card_info.BlockNbr, card_info.BlockSize);
    return true;
#endif
#else
    return false;
#endif
}

osThreadId_t task_handle_user_input;

extern "C" void task_user_input(void *argument);

extern "C" void init()
{
    osThreadAttr_t defaultTask_attributes = { };
    defaultTask_attributes.name = "task_user_input";
    defaultTask_attributes.stack_size = 256 * 4;
    defaultTask_attributes.priority = (osPriority_t) osPriorityNormal;

    task_handle_user_input = osThreadNew(task_user_input, NULL, &defaultTask_attributes);
}

//------------------------------------------------------------------------------
class AudioPlayerTerminalGui_t
{
public:
};

AudioPlayer_t *g_audio_player = nullptr;

//------------------------------------------------------------------------------
bool shell_cmd_play(FILE *f, ShellCmd_t *cmd, const char *s)
{
    const char *fname = cmd->get_str_arg(s, 1);
    static char *abs_path = nullptr;
     
    if (!abs_path) abs_path = new char[FF_MAX_LFN];
    abs_path[0] = 0;
    bool is_success = FileSystemUtils_t::relativePathToAbsolute(fname, abs_path);
    fprintf(f, "Abs path: %s" ENDL, abs_path);
    if (!is_success) return false;

    if (g_audio_player)
    {
        fprintf(f, "Previous song is still playing, stopping..." ENDL);
        g_audio_player->m_controller.stop();
        delete g_audio_player;
        g_audio_player = nullptr;
    }

    AudioPlayerInputs_t inputs;
    inputs.button_pause = &button1;
    inputs.button_next = &button2;
    g_audio_player = new AudioPlayer_t(inputs);

    bool res = g_audio_player->m_controller.playSong(abs_path);
    if (!res)
    {
        delete g_audio_player;
        g_audio_player = nullptr;
        if (abs_path) 
        {
            delete abs_path;
            abs_path = nullptr;
        }
    }
    return res;
}

bool shell_wav_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    if (g_audio_player)
    {
        g_audio_player->m_controller.printStat();
        return true;
    }
    return false;    
}

bool shell_heap_stat(FILE *f, ShellCmd_t *cmd, const char *s)
{
    struct mallinfo heap_info = mallinfo();
    fprintf(f, "arena=%d" ENDL, heap_info.arena);  /* Non-mmapped space allocated (bytes) */
    fprintf(f, "ordblks=%d" ENDL, heap_info.ordblks);  /* Number of free chunks */
    fprintf(f, "smblks=%d" ENDL, heap_info.smblks);  /* Number of free fastbin blocks */
    fprintf(f, "hblks=%d" ENDL, heap_info.hblks);  /* Number of mmapped regions */
    fprintf(f, "hblkhd=%d" ENDL, heap_info.hblkhd);  /* Space allocated in mmapped regions (bytes) */
    fprintf(f, "usmblks=%d" ENDL, heap_info.usmblks);  /* Maximum total allocated space (bytes) */
    fprintf(f, "fsmblks=%d" ENDL, heap_info.fsmblks);  /* Space in freed fastbin blocks (bytes) */
    fprintf(f, "uordblks=%d" ENDL, heap_info.uordblks);  /* Total allocated space (bytes) */
    fprintf(f, "fordblks=%d" ENDL, heap_info.fordblks);  /* Total free space (bytes) */
    fprintf(f, "keepcost=%d" ENDL, heap_info.keepcost);  /* Top-most, releasable space (bytes) */
    return true;
}

void init_shell(FILE *device=stdout)
{
    shell.add_command(ShellCmd_t("cls", "Clear screen", shell_cmd_clear_screen));
#if (EN_TETRIS == 1)
    shell.add_command(ShellCmd_t("tetris", "Tetris!", shell_cmd_tetris));
#endif
#if (EN_SD_CARD_READ_WRITE_SHELL_CMDS == 1)
    shell.add_command(ShellCmd_t("sdrd", "SD card read", shell_cmd_sd_card_read));
    shell.add_command(ShellCmd_t("sdwr", "SD card write", shell_cmd_sd_card_write));
#endif
#if (EN_FATFS_SHELL_CMDS == 1)
    shell.add_command(ShellCmd_t("ls", "Print conents of the current directory", FileSystemUtils_t::shell_cmd_ls));
    shell.add_command(ShellCmd_t("cd", "Change directory", FileSystemUtils_t::shell_cmd_cd));
    shell.add_command(ShellCmd_t("pwd", "Print current directory", FileSystemUtils_t::shell_cmd_pwd));
#endif    
    shell.add_command(ShellCmd_t("led", "LED control", shell_cmd_led));

#if (EN_AUDIO_SHELL_CMDS)    
    shell.add_command(ShellCmd_t("dac_test", "Test DAC", shell_cmd_test_dac));
    shell.add_command(ShellCmd_t("volume", "Configure audio volume", shell_cmd_set_volume));
    shell.add_command(ShellCmd_t("play", "", shell_cmd_play));
    shell.add_command(ShellCmd_t("wavstat", "", shell_wav_stat));
    
    shell.add_command(ShellCmd_t("wavstat", "", AudioPlayerShelUi_t::shell_cmd_auplayer_pause));
    
#endif

    shell.add_command(ShellCmd_t("heap", "", shell_heap_stat));

    shell.set_device(device);
    shell.print_prompt();
}

EpaperWeAct_Driver_t epaper(400, 300);
GpioPinPortG_t<2> pin_epaper_busy;
GpioPinPortG_t<3> pin_epaper_rst;
GpioPinPortG_t<4> pin_epaper_dc;
GpioPinPortG_t<5> pin_epaper_cs;
GpioPinPortG_t<6> pin_epaper_scl;
GpioPinPortG_t<7> pin_epaper_sda;

Font_t font(FreeMono12pt7b);
//Font_t font(FreeMonoBoldOblique12pt7b);
TextRendererWithBinaryBuffer_t text_renderer(epaper, font);

const char *book =
"Introduction\n"
"\n"
"Folklore, legends, myths and fairy tales have followed childhood through the ages, for every healthy youngster has a wholesome and instinctive love for stories fantastic, marvelous and manifestly unreal. The winged fairies of Grimm and Andersen have brought more happiness to childish hearts than all other human creations.\n"
"\n"
"Yet the old time fairy tale, having served for generations, may now be classed as \"historical\" in the children's library; for the time has come for a series of newer \"wonder tales\" in which the stereotyped genie, dwarf and fairy are eliminated, together with all the horrible and blood-curdling incidents devised by their authors to point a fearsome moral to each tale. Modern education includes morality; therefore the modern child seeks only entertainment in its wonder tales and gladly dispenses with all disagreeable incident.\n"
"\n"
"Having this thought in mind, the story of \"The Wonderful Wizard of Oz\" was written solely to please children of today. It aspires to being a modernized fairy tale, in which the wonderment and joy are retained and the heartaches and nightmares are left out.\n"
"\n"
"L. Frank Baum\n"
"Chicago, April, 1900.\n"
"The Wonderful Wizard of Oz\n"
"Chapter I\n"
"The Cyclone\n"
"\n"
"Dorothy lived in the midst of the great Kansas prairies, with Uncle Henry, who was a farmer, and Aunt Em, who was the farmer's wife. Their house was small, for the lumber to build it had to be carried by wagon many miles. There were four walls, a floor and a roof, which made one room; and this room contained a rusty looking cookstove, a cupboard for the dishes, a table, three or four chairs, and the beds. Uncle Henry and Aunt Em had a big bed in one corner, and Dorothy a little bed in another corner. There was no garret at all, and no cellar—except a small hole dug in the ground, called a cyclone cellar, where the family could go in case one of those great whirlwinds arose, mighty enough to crush any building in its path. It was reached by a trap door in the middle of the floor, from which a ladder led down into the small, dark hole.\n"
"\n"
"When Dorothy stood in the doorway and looked around, she could see nothing but the great gray prairie on every side. Not a tree nor a house broke the broad sweep of flat country that reached to the edge of the sky in all directions. The sun had baked the plowed land into a gray mass, with little cracks running through it. Even the grass was not green, for the sun had burned the tops of the long blades until they were the same gray color to be seen everywhere. Once the house had been painted, but the sun blistered the paint and the rains washed it away, and now the house was as dull and gray as everything else.\n"
"\n"
"When Aunt Em came there to live she was a young, pretty wife. The sun and wind had changed her, too. They had taken the sparkle from her eyes and left them a sober gray; they had taken the red from her cheeks and lips, and they were gray also. She was thin and gaunt, and never smiled now. When Dorothy, who was an orphan, first came to her, Aunt Em had been so startled by the child's laughter that she would scream and press her hand upon her heart whenever Dorothy's merry voice reached her ears; and she still looked at the little girl with wonder that she could find anything to laugh at.\n"
"\n"
"Uncle Henry never laughed. He worked hard from morning till night and did not know what joy was. He was gray also, from his long beard to his rough boots, and he looked stern and solemn, and rarely spoke.\n"
"\n"
"It was Toto that made Dorothy laugh, and saved her from growing as gray as her other surroundings. Toto was not gray; he was a little black dog, with long silky hair and small black eyes that twinkled merrily on either side of his funny, wee nose. Toto played all day long, and Dorothy played with him, and loved him dearly.\n"
"\n"
"Today, however, they were not playing. Uncle Henry sat upon the doorstep and looked anxiously at the sky, which was even grayer than usual. Dorothy stood in the door with Toto in her arms, and looked at the sky too. Aunt Em was washing the dishes.\n"
"\n"
"From the far north they heard a low wail of the wind, and Uncle Henry and Dorothy could see where the long grass bowed in waves before the coming storm. There now came a sharp whistling in the air from the south, and as they turned their eyes that way they saw ripples in the grass coming from that direction also.\n"
"\n"
"Suddenly Uncle Henry stood up.\n"
"\n"
"\"There's a cyclone coming, Em,\" he called to his wife. \"I'll go look after the stock.\" Then he ran toward the sheds where the cows and horses were kept.\n"
"\n"
"Aunt Em dropped her work and came to the door. One glance told her of the danger close at hand.\n"
"\n"
"\"Quick, Dorothy!\" she screamed. \"Run for the cellar!\"\n"
"\n"
"Toto jumped out of Dorothy's arms and hid under the bed, and the girl started to get him. Aunt Em, badly frightened, threw open the trap door in the floor and climbed down the ladder into the small, dark hole. Dorothy caught Toto at last and started to follow her aunt. When she was halfway across the room there came a great shriek from the wind, and the house shook so hard that she lost her footing and sat down suddenly upon the floor.\n"
"\n"
"Then a strange thing happened.\n"
"\n"
"The house whirled around two or three times and rose slowly through the air. Dorothy felt as if she were going up in a balloon.\n"
"\n"
"The north and south winds met where the house stood, and made it the exact center of the cyclone. In the middle of a cyclone the air is generally still, but the great pressure of the wind on every side of the house raised it up higher and higher, until it was at the very top of the cyclone; and there it remained and was carried miles and miles away as easily as you could carry a feather.\n"
"\n"
"It was very dark, and the wind howled horribly around her, but Dorothy found she was riding quite easily. After the first few whirls around, and one other time when the house tipped badly, she felt as if she were being rocked gently, like a baby in a cradle.\n"
"\n"
"Toto did not like it. He ran about the room, now here, now there, barking loudly; but Dorothy sat quite still on the floor and waited to see what would happen.\n"
"\n"
"Once Toto got too near the open trap door, and fell in; and at first the little girl thought she had lost him. But soon she saw one of his ears sticking up through the hole, for the strong pressure of the air was keeping him up so that he could not fall. She crept to the hole, caught Toto by the ear, and dragged him into the room again, afterward closing the trap door so that no more accidents could happen.\n"
"\n"
"Hour after hour passed away, and slowly Dorothy got over her fright; but she felt quite lonely, and the wind shrieked so loudly all about her that she nearly became deaf. At first she had wondered if she would be dashed to pieces when the house fell again; but as the hours passed and nothing terrible happened, she stopped worrying and resolved to wait calmly and see what the future would bring. At last she crawled over the swaying floor to her bed, and lay down upon it; and Toto followed and lay down beside her.\n"
"\n"
"In spite of the swaying of the house and the wailing of the wind, Dorothy soon closed her eyes and fell fast asleep.\n"
"Chapter II\n"
"The Council with the Munchkins\n"
"\n"
"She was awakened by a shock, so sudden and severe that if Dorothy had not been lying on the soft bed she might have been hurt. As it was, the jar made her catch her breath and wonder what had happened; and Toto put his cold little nose into her face and whined dismally. Dorothy sat up and noticed that the house was not moving; nor was it dark, for the bright sunshine came in at the window, flooding the little room. She sprang from her bed and with Toto at her heels ran and opened the door.\n"
"\n"
"The little girl gave a cry of amazement and looked about her, her eyes growing bigger and bigger at the wonderful sights she saw.\n"
"\n"
"The cyclone had set the house down very gently—for a cyclone—in the midst of a country of marvelous beauty. There were lovely patches of greensward all about, with stately trees bearing rich and luscious fruits. Banks of gorgeous flowers were on every hand, and birds with rare and brilliant plumage sang and fluttered in the trees and bushes. A little way off was a small brook, rushing and sparkling along between green banks, and murmuring in a voice very grateful to a little girl who had lived so long on the dry, gray prairies.\n"
"\n"
"While she stood looking eagerly at the strange and beautiful sights, she noticed coming toward her a group of the queerest people she had ever seen. They were not as big as the grown folk she had always been used to; but neither were they very small. In fact, they seemed about as tall as Dorothy, who was a well-grown child for her age, although they were, so far as looks go, many years older.\n"
"\n"
"Three were men and one a woman, and all were oddly dressed. They wore round hats that rose to a small point a foot above their heads, with little bells around the brims that tinkled sweetly as they moved. The hats of the men were blue; the little woman's hat was white, and she wore a white gown that hung in pleats from her shoulders. Over it were sprinkled little stars that glistened in the sun like diamonds. The men were dressed in blue, of the same shade as their hats, and wore well-polished boots with a deep roll of blue at the tops. The men, Dorothy thought, were about as old as Uncle Henry, for two of them had beards. But the little woman was doubtless much older. Her face was covered with wrinkles, her hair was nearly white, and she walked rather stiffly.\n"
"\n"
"When these people drew near the house where Dorothy was standing in the doorway, they paused and whispered among themselves, as if afraid to come farther. But the little old woman walked up to Dorothy, made a low bow and said, in a sweet voice:\n"
"\n"
"\"You are welcome, most noble Sorceress, to the land of the Munchkins. We are so grateful to you for having killed the Wicked Witch of the East, and for setting our people free from bondage.\"\n"
"\n"
"Dorothy listened to this speech with wonder. What could the little woman possibly mean by calling her a sorceress, and saying she had killed the Wicked Witch of the East? Dorothy was an innocent, harmless little girl, who had been carried by a cyclone many miles from home; and she had never killed anything in all her life.\n"
"\n"
"But the little woman evidently expected her to answer; so Dorothy said, with hesitation, \"You are very kind, but there must be some mistake. I have not killed anything.\"\n"
"\n"
"\"Your house did, anyway,\" replied the little old woman, with a laugh, \"and that is the same thing. See!\" she continued, pointing to the corner of the house. \"There are her two feet, still sticking out from under a block of wood.\"\n"
"\n"
"Dorothy looked, and gave a little cry of fright. There, indeed, just under the corner of the great beam the house rested on, two feet were sticking out, shod in silver shoes with pointed toes.\n"
"\n"
"\"Oh, dear! Oh, dear!\" cried Dorothy, clasping her hands together in dismay. \"The house must have fallen on her. Whatever shall we do?\"\n"
"\n"
"\"There is nothing to be done,\" said the little woman calmly.\n"
"\n"
"\"But who was she?\" asked Dorothy.\n"
"\n"
"\"She was the Wicked Witch of the East, as I said,\" answered the little woman. \"She has held all the Munchkins in bondage for many years, making them slave for her night and day. Now they are all set free, and are grateful to you for the favor.\"\n"
"\n"
"\"Who are the Munchkins?\" inquired Dorothy.\n"
"\n"
"\"They are the people who live in this land of the East where the Wicked Witch ruled.\"\n"
"\n"
"\"Are you a Munchkin?\" asked Dorothy.\n"
"\n"
"\"No, but I am their friend, although I live in the land of the North. When they saw the Witch of the East was dead the Munchkins sent a swift messenger to me, and I came at once. I am the Witch of the North.\"\n"
"\n"
"\"Oh, gracious!\" cried Dorothy. \"Are you a real witch?\"\n"
"\n"
"\"Yes, indeed,\" answered the little woman. \"But I am a good witch, and the people love me. I am not as powerful as the Wicked Witch was who ruled here, or I should have set the people free myself.\"\n"
"\n"
"\"But I thought all witches were wicked,\" said the girl, who was half frightened at facing a real witch.\n"
"\n"
"\"Oh, no, that is a great mistake. There were only four witches in all the Land of Oz, and two of them, those who live in the North and the South, are good witches. I know this is true, for I am one of them myself, and cannot be mistaken. Those who dwelt in the East and the West were, indeed, wicked witches; but now that you have killed one of them, there is but one Wicked Witch in all the Land of Oz—the one who lives in the West.\"\n"
"\n"
"\"But,\" said Dorothy, after a moment's thought, \"Aunt Em has told me that the witches were all dead—years and years ago.\"\n"
"\n"
"\"Who is Aunt Em?\" inquired the little old woman.\n"
"\n"
"\"She is my aunt who lives in Kansas, where I came from.\"\n"
"\n"
"The Witch of the North seemed to think for a time, with her head bowed and her eyes upon the ground. Then she looked up and said, \"I do not know where Kansas is, for I have never heard that country mentioned before. But tell me, is it a civilized country?\"\n"
"\n"
"\"Oh, yes,\" replied Dorothy.\n"
"\n"
"\"Then that accounts for it. In the civilized countries I believe there are no witches left, nor wizards, nor sorceresses, nor magicians. But, you see, the Land of Oz has never been civilized, for we are cut off from all the rest of the world. Therefore we still have witches and wizards amongst us.\"\n"
"\n"
"\"Who are the wizards?\" asked Dorothy.\n"
"\n"
"\"Oz himself is the Great Wizard,\" answered the Witch, sinking her voice to a whisper. \"He is more powerful than all the rest of us together. He lives in the City of Emeralds.\"\n"
"\n"
"Dorothy was going to ask another question, but just then the Munchkins, who had been standing silently by, gave a loud shout and pointed to the corner of the house where the Wicked Witch had been lying.\n"
"\n"
"\"What is it?\" asked the little old woman, and looked, and began to laugh. The feet of the dead Witch had disappeared entirely, and nothing was left but the silver shoes.\n"
"\n"
"\"She was so old,\" explained the Witch of the North, \"that she dried up quickly in the sun. That is the end of her. But the silver shoes are yours, and you shall have them to wear.\" She reached down and picked up the shoes, and after shaking the dust out of them handed them to Dorothy.\n"
"\n"
"\"The Witch of the East was proud of those silver shoes,\" said one of the Munchkins, \"and there is some charm connected with them; but what it is we never knew.\"\n"
"\n"
"Dorothy carried the shoes into the house and placed them on the table. Then she came out again to the Munchkins and said:\n"
"\n"
"\"I am anxious to get back to my aunt and uncle, for I am sure they will worry about me. Can you help me find my way?\"\n"
"\n"
"The Munchkins and the Witch first looked at one another, and then at Dorothy, and then shook their heads.\n"
"\n"
"\"At the East, not far from here,\" said one, \"there is a great desert, and none could live to cross it.\"\n"
"\n"
"\"It is the same at the South,\" said another, \"for I have been there and seen it. The South is the country of the Quadlings.\"\n"
"\n"
"\"I am told,\" said the third man, \"that it is the same at the West. And that country, where the Winkies live, is ruled by the Wicked Witch of the West, who would make you her slave if you passed her way.\"\n"
"\n"
"\"The North is my home,\" said the old lady, \"and at its edge is the same great desert that surrounds this Land of Oz. I'm afraid, my dear, you will have to live with us.\"\n"
"\n"
"Dorothy began to sob at this, for she felt lonely among all these strange people. Her tears seemed to grieve the kind-hearted Munchkins, for they immediately took out their handkerchiefs and began to weep also. As for the little old woman, she took off her cap and balanced the point on the end of her nose, while she counted \"One, two, three\" in a solemn voice. At once the cap changed to a slate, on which was written in big, white chalk marks:\n"
"\n"
"\"LET DOROTHY GO TO THE CITY OF EMERALDS\"\n"
"\n"
"The little old woman took the slate from her nose, and having read the words on it, asked, \"Is your name Dorothy, my dear?\"\n"
"\n"
"\"Yes,\" answered the child, looking up and drying her tears.\n"
"\n"
"\"Then you must go to the City of Emeralds. Perhaps Oz will help you.\"\n"
"\n"
"\"Where is this city?\" asked Dorothy.\n"
"\n"
"\"It is exactly in the center of the country, and is ruled by Oz, the Great Wizard I told you of.\"\n"
"\n"
"\"Is he a good man?\" inquired the girl anxiously.\n"
"\n"
"\"He is a good Wizard. Whether he is a man or not I cannot tell, for I have never seen him.\"\n"
"\n"
"\"How can I get there?\" asked Dorothy.\n"
"\n"
"\"You must walk. It is a long journey, through a country that is sometimes pleasant and sometimes dark and terrible. However, I will use all the magic arts I know of to keep you from harm.\"\n"
"\n"
"\"Won't you go with me?\" pleaded the girl, who had begun to look upon the little old woman as her only friend.\n"
"\n"
"\"No, I cannot do that,\" she replied, \"but I will give you my kiss, and no one will dare injure a person who has been kissed by the Witch of the North.\"\n"
"\n"
"She came close to Dorothy and kissed her gently on the forehead. Where her lips touched the girl they left a round, shining mark, as Dorothy found out soon after.\n"
"\n"
"\"The road to the City of Emeralds is paved with yellow brick,\" said the Witch, \"so you cannot miss it. When you get to Oz do not be afraid of him, but tell your story and ask him to help you. Good-bye, my dear.\"\n"
"\n"
"The three Munchkins bowed low to her and wished her a pleasant journey, after which they walked away through the trees. The Witch gave Dorothy a friendly little nod, whirled around on her left heel three times, and straightway disappeared, much to the surprise of little Toto, who barked after her loudly enough when she had gone, because he had been afraid even to growl while she stood by.\n"
"\n"
"But Dorothy, knowing her to be a witch, had expected her to disappear in just that way, and was not surprised in the least.\n"
"Chapter III\n"
"How Dorothy Saved the Scarecrow\n"
"\n"
"When Dorothy was left alone she began to feel hungry. So she went to the cupboard and cut herself some bread, which she spread with butter. She gave some to Toto, and taking a pail from the shelf she carried it down to the little brook and filled it with clear, sparkling water. Toto ran over to the trees and began to bark at the birds sitting there. Dorothy went to get him, and saw such delicious fruit hanging from the branches that she gathered some of it, finding it just what she wanted to help out her breakfast.\n"
"\n"
"Then she went back to the house, and having helped herself and Toto to a good drink of the cool, clear water, she set about making ready for the journey to the City of Emeralds.\n"
"\n"
"Dorothy had only one other dress, but that happened to be clean and was hanging on a peg beside her bed. It was gingham, with checks of white and blue; and although the blue was somewhat faded with many washings, it was still a pretty frock. The girl washed herself carefully, dressed herself in the clean gingham, and tied her pink sunbonnet on her head. She took a little basket and filled it with bread from the cupboard, laying a white cloth over the top. Then she looked down at her feet and noticed how old and worn her shoes were.\n"
"\n"
"\"They surely will never do for a long journey, Toto,\" she said. And Toto looked up into her face with his little black eyes and wagged his tail to show he knew what she meant.\n"
"\n"
"At that moment Dorothy saw lying on the table the silver shoes that had belonged to the Witch of the East.\n"
"\n"
"\"I wonder if they will fit me,\" she said to Toto. \"They would be just the thing to take a long walk in, for they could not wear out.\"\n"
"\n"
"She took off her old leather shoes and tried on the silver ones, which fitted her as well as if they had been made for her.\n"
"\n"
"Finally she picked up her basket.\n"
"\n"
"\"Come along, Toto,\" she said. \"We will go to the Emerald City and ask the Great Oz how to get back to Kansas again.\"\n"
"\n"
"She closed the door, locked it, and put the key carefully in the pocket of her dress. And so, with Toto trotting along soberly behind her, she started on her journey.\n"
"\n"
"There were several roads nearby, but it did not take her long to find the one paved with yellow bricks. Within a short time she was walking briskly toward the Emerald City, her silver shoes tinkling merrily on the hard, yellow road-bed. The sun shone bright and the birds sang sweetly, and Dorothy did not feel nearly so bad as you might think a little girl would who had been suddenly whisked away from her own country and set down in the midst of a strange land.\n"
"\n"
"She was surprised, as she walked along, to see how pretty the country was about her. There were neat fences at the sides of the road, painted a dainty blue color, and beyond them were fields of grain and vegetables in abundance. Evidently the Munchkins were good farmers and able to raise large crops. Once in a while she would pass a house, and the people came out to look at her and bow low as she went by; for everyone knew she had been the means of destroying the Wicked Witch and setting them free from bondage. The houses of the Munchkins were odd-looking dwellings, for each was round, with a big dome for a roof. All were painted blue, for in this country of the East blue was the favorite color.\n"
"\n"
"Toward evening, when Dorothy was tired with her long walk and began to wonder where she should pass the night, she came to a house rather larger than the rest. On the green lawn before it many men and women were dancing. Five little fiddlers played as loudly as possible, and the people were laughing and singing, while a big table near by was loaded with delicious fruits and nuts, pies and cakes, and many other good things to eat.\n"
"\n"
"The people greeted Dorothy kindly, and invited her to supper and to pass the night with them; for this was the home of one of the richest Munchkins in the land, and his friends were gathered with him to celebrate their freedom from the bondage of the Wicked Witch.\n"
"\n"
"Dorothy ate a hearty supper and was waited upon by the rich Munchkin himself, whose name was Boq. Then she sat upon a settee and watched the people dance.\n"
"\n"
"When Boq saw her silver shoes he said, \"You must be a great sorceress.\"\n"
"\n"
"\"Why?\" asked the girl.\n"
"\n"
"\"Because you wear silver shoes and have killed the Wicked Witch. Besides, you have white in your frock, and only witches and sorceresses wear white.\"\n"
"\n"
"\"My dress is blue and white checked,\" said Dorothy, smoothing out the wrinkles in it.\n"
"\n"
"\"It is kind of you to wear that,\" said Boq. \"Blue is the color of the Munchkins, and white is the witch color. So we know you are a friendly witch.\"\n"
"\n"
"Dorothy did not know what to say to this, for all the people seemed to think her a witch, and she knew very well she was only an ordinary little girl who had come by the chance of a cyclone into a strange land.\n"
"\n"
"When she had tired watching the dancing, Boq led her into the house, where he gave her a room with a pretty bed in it. The sheets were made of blue cloth, and Dorothy slept soundly in them till morning, with Toto curled up on the blue rug beside her.\n"
"\n"
"She ate a hearty breakfast, and watched a wee Munchkin baby, who played with Toto and pulled his tail and crowed and laughed in a way that greatly amused Dorothy. Toto was a fine curiosity to all the people, for they had never seen a dog before.\n"
"\n"
"\"How far is it to the Emerald City?\" the girl asked.\n"
"\n"
"\"I do not know,\" answered Boq gravely, \"for I have never been there. It is better for people to keep away from Oz, unless they have business with him. But it is a long way to the Emerald City, and it will take you many days. The country here is rich and pleasant, but you must pass through rough and dangerous places before you reach the end of your journey.\"\n"
"\n"
"This worried Dorothy a little, but she knew that only the Great Oz could help her get to Kansas again, so she bravely resolved not to turn back.\n"
"\n"
"She bade her friends good-bye, and again started along the road of yellow brick. When she had gone several miles she thought she would stop to rest, and so climbed to the top of the fence beside the road and sat down. There was a great cornfield beyond the fence, and not far away she saw a Scarecrow, placed high on a pole to keep the birds from the ripe corn.\n"
"\n"
"Dorothy leaned her chin upon her hand and gazed thoughtfully at the Scarecrow. Its head was a small sack stuffed with straw, with eyes, nose, and mouth painted on it to represent a face. An old, pointed blue hat, that had belonged to some Munchkin, was perched on his head, and the rest of the figure was a blue suit of clothes, worn and faded, which had also been stuffed with straw. On the feet were some old boots with blue tops, such as every man wore in this country, and the figure was raised above the stalks of corn by means of the pole stuck up its back.\n"
"\n"
"While Dorothy was looking earnestly into the queer, painted face of the Scarecrow, she was surprised to see one of the eyes slowly wink at her. She thought she must have been mistaken at first, for none of the scarecrows in Kansas ever wink; but presently the figure nodded its head to her in a friendly way. Then she climbed down from the fence and walked up to it, while Toto ran around the pole and barked.\n"
"\n"
"\"Good day,\" said the Scarecrow, in a rather husky voice.\n"
"\n"
"\"Did you speak?\" asked the girl, in wonder.\n"
"\n"
"\"Certainly,\" answered the Scarecrow. \"How do you do?\"\n"
"\n"
"\"I'm pretty well, thank you,\" replied Dorothy politely. \"How do you do?\"\n"
"\n"
"\"I'm not feeling well,\" said the Scarecrow, with a smile, \"for it is very tedious being perched up here night and day to scare away crows.\"\n"
"\n"
"\"Can't you get down?\" asked Dorothy.\n"
"\n"
"\"No, for this pole is stuck up my back. If you will please take away the pole I shall be greatly obliged to you.\"\n"
"\n"
"Dorothy reached up both arms and lifted the figure off the pole, for, being stuffed with straw, it was quite light.\n"
"\n"
"\"Thank you very much,\" said the Scarecrow, when he had been set down on the ground. \"I feel like a new man.\"\n"
"\n"
"Dorothy was puzzled at this, for it sounded queer to hear a stuffed man speak, and to see him bow and walk along beside her.\n"
"\n"
"\"Who are you?\" asked the Scarecrow when he had stretched himself and yawned. \"And where are you going?\"\n"
"\n"
"\"My name is Dorothy,\" said the girl, \"and I am going to the Emerald City, to ask the Great Oz to send me back to Kansas.\"\n"
"\n"
"\"Where is the Emerald City?\" he inquired. \"And who is Oz?\"\n"
"\n"
"\"Why, don't you know?\" she returned, in surprise.\n"
"\n"
"\"No, indeed. I don't know anything. You see, I am stuffed, so I have no brains at all,\" he answered sadly.\n"
"\n"
"\"Oh,\" said Dorothy, \"I'm awfully sorry for you.\"\n"
"\n"
"\"Do you think,\" he asked, \"if I go to the Emerald City with you, that Oz would give me some brains?\"\n"
"\n"
"\"I cannot tell,\" she returned, \"but you may come with me, if you like. If Oz will not give you any brains you will be no worse off than you are now.\"\n"
"\n"
"\"That is true,\" said the Scarecrow. \"You see,\" he continued confidentially, \"I don't mind my legs and arms and body being stuffed, because I cannot get hurt. If anyone treads on my toes or sticks a pin into me, it doesn't matter, for I can't feel it. But I do not want people to call me a fool, and if my head stays stuffed with straw instead of with brains, as yours is, how am I ever to know anything?\"\n"
"\n"
"\"I understand how you feel,\" said the little girl, who was truly sorry for him. \"If you will come with me I'll ask Oz to do all he can for you.\"\n"
"\n"
"\"Thank you,\" he answered gratefully.\n"
"\n"
"They walked back to the road. Dorothy helped him over the fence, and they started along the path of yellow brick for the Emerald City.\n"
"\n"
"Toto did not like this addition to the party at first. He smelled around the stuffed man as if he suspected there might be a nest of rats in the straw, and he often growled in an unfriendly way at the Scarecrow.\n"
"\n"
"\"Don't mind Toto,\" said Dorothy to her new friend. \"He never bites.\"\n"
"\n"
"\"Oh, I'm not afraid,\" replied the Scarecrow. \"He can't hurt the straw. Do let me carry that basket for you. I shall not mind it, for I can't get tired. I'll tell you a secret,\" he continued, as he walked along. \"There is only one thing in the world I am afraid of.\"\n"
"\n"
"\"What is that?\" asked Dorothy; \"the Munchkin farmer who made you?\"\n"
"\n"
"\"No,\" answered the Scarecrow; \"it's a lighted match.\"\n"
"Chapter IV\n"
"The Road Through the Forest\n"
"\n"
"After a few hours the road began to be rough, and the walking grew so difficult that the Scarecrow often stumbled over the yellow bricks, which were here very uneven. Sometimes, indeed, they were broken or missing altogether, leaving holes that Toto jumped across and Dorothy walked around. As for the Scarecrow, having no brains, he walked straight ahead, and so stepped into the holes and fell at full length on the hard bricks. It never hurt him, however, and Dorothy would pick him up and set him upon his feet again, while he joined her in laughing merrily at his own mishap.\n"
"\n"
"The farms were not nearly so well cared for here as they were farther back. There were fewer houses and fewer fruit trees, and the farther they went the more dismal and lonesome the country became.\n"
"\n"
"At noon they sat down by the roadside, near a little brook, and Dorothy opened her basket and got out some bread. She offered a piece to the Scarecrow, but he refused.\n"
"\n"
"\"I am never hungry,\" he said, \"and it is a lucky thing I am not, for my mouth is only painted, and if I should cut a hole in it so I could eat, the straw I am stuffed with would come out, and that would spoil the shape of my head.\"\n"
"\n"
"Dorothy saw at once that this was true, so she only nodded and went on eating her bread.\n"
"\n"
"\"Tell me something about yourself and the country you came from,\" said the Scarecrow, when she had finished her dinner. So she told him all about Kansas, and how gray everything was there, and how the cyclone had carried her to this queer Land of Oz.\n"
"\n"
"The Scarecrow listened carefully, and said, \"I cannot understand why you should wish to leave this beautiful country and go back to the dry, gray place you call Kansas.\"\n"
"\n"
"\"That is because you have no brains\" answered the girl. \"No matter how dreary and gray our homes are, we people of flesh and blood would rather live there than in any other country, be it ever so beautiful. There is no place like home.\"\n"
"\n"
"The Scarecrow sighed.\n"
"\n"
"\"Of course I cannot understand it,\" he said. \"If your heads were stuffed with straw, like mine, you would probably all live in the beautiful places, and then Kansas would have no people at all. It is fortunate for Kansas that you have brains.\"\n"
"\n"
"\"Won't you tell me a story, while we are resting?\" asked the child.\n"
"\n"
"The Scarecrow looked at her reproachfully, and answered:\n"
"\n"
"\"My life has been so short that I really know nothing whatever. I was only made day before yesterday. What happened in the world before that time is all unknown to me. Luckily, when the farmer made my head, one of the first things he did was to paint my ears, so that I heard what was going on. There was another Munchkin with him, and the first thing I heard was the farmer saying, 'How do you like those ears?'\n"
"\n"
"\"'They aren't straight,'\" answered the other.\n"
"\n"
"\"'Never mind,'\" said the farmer. \"'They are ears just the same,'\" which was true enough.\n"
"\n"
"\"'Now I'll make the eyes,'\" said the farmer. So he painted my right eye, and as soon as it was finished I found myself looking at him and at everything around me with a great deal of curiosity, for this was my first glimpse of the world.\n"
"\n"
"\"'That's a rather pretty eye,'\" remarked the Munchkin who was watching the farmer. \"'Blue paint is just the color for eyes.'\n"
"\n"
"\"'I think I'll make the other a little bigger,'\" said the farmer. And when the second eye was done I could see much better than before. Then he made my nose and my mouth. But I did not speak, because at that time I didn't know what a mouth was for. I had the fun of watching them make my body and my arms and legs; and when they fastened on my head, at last, I felt very proud, for I thought I was just as good a man as anyone.\n"
"\n"
"\"'This fellow will scare the crows fast enough,' said the farmer. 'He looks just like a man.'\n"
"\n"
"\"'Why, he is a man,' said the other, and I quite agreed with him. The farmer carried me under his arm to the cornfield, and set me up on a tall stick, where you found me. He and his friend soon after walked away and left me alone.\n"
"\n"
"\"I did not like to be deserted this way. So I tried to walk after them. But my feet would not touch the ground, and I was forced to stay on that pole. It was a lonely life to lead, for I had nothing to think of, having been made such a little while before. Many crows and other birds flew into the cornfield, but as soon as they saw me they flew away again, thinking I was a Munchkin; and this pleased me and made me feel that I was quite an important person. By and by an old crow flew near me, and after looking at me carefully he perched upon my shoulder and said:\n"
"\n"
"\"'I wonder if that farmer thought to fool me in this clumsy manner. Any crow of sense could see that you are only stuffed with straw.' Then he hopped down at my feet and ate all the corn he wanted. The other birds, seeing he was not harmed by me, came to eat the corn too, so in a short time there was a great flock of them about me.\n"
"\n"
"\"I felt sad at this, for it showed I was not such a good Scarecrow after all; but the old crow comforted me, saying, 'If you only had brains in your head you would be as good a man as any of them, and a better man than some of them. Brains are the only things worth having in this world, no matter whether one is a crow or a man.'\n"
"\n"
"\"After the crows had gone I thought this over, and decided I would try hard to get some brains. By good luck you came along and pulled me off the stake, and from what you say I am sure the Great Oz will give me brains as soon as we get to the Emerald City.\"\n"
"\n"
"\"I hope so,\" said Dorothy earnestly, \"since you seem anxious to have them.\"\n"
"\n"
"\"Oh, yes; I am anxious,\" returned the Scarecrow. \"It is such an uncomfortable feeling to know one is a fool.\"\n"
"\n"
"\"Well,\" said the girl, \"let us go.\" And she handed the basket to the Scarecrow.\n"
"\n"
"There were no fences at all by the roadside now, and the land was rough and untilled. Toward evening they came to a great forest, where the trees grew so big and close together that their branches met over the road of yellow brick. It was almost dark under the trees, for the branches shut out the daylight; but the travelers did not stop, and went on into the forest.\n"
"\n"
"\"If this road goes in, it must come out,\" said the Scarecrow, \"and as the Emerald City is at the other end of the road, we must go wherever it leads us.\"\n"
"\n"
"\"Anyone would know that,\" said Dorothy.\n"
"\n"
"\"Certainly; that is why I know it,\" returned the Scarecrow. \"If it required brains to figure it out, I never should have said it.\"\n"
"\n"
"After an hour or so the light faded away, and they found themselves stumbling along in the darkness. Dorothy could not see at all, but Toto could, for some dogs see very well in the dark; and the Scarecrow declared he could see as well as by day. So she took hold of his arm and managed to get along fairly well.\n"
"\n"
"\"If you see any house, or any place where we can pass the night,\" she said, \"you must tell me; for it is very uncomfortable walking in the dark.\"\n"
"\n"
"Soon after the Scarecrow stopped.\n"
"\n"
"\"I see a little cottage at the right of us,\" he said, \"built of logs and branches. Shall we go there?\"\n"
"\n"
"\"Yes, indeed,\" answered the child. \"I am all tired out.\"\n"
"\n"
"So the Scarecrow led her through the trees until they reached the cottage, and Dorothy entered and found a bed of dried leaves in one corner. She lay down at once, and with Toto beside her soon fell into a sound sleep. The Scarecrow, who was never tired, stood up in another corner and waited patiently until morning came.\n"
"Chapter V\n"
"The Rescue of the Tin Woodman\n"
"\n"
"When Dorothy awoke the sun was shining through the trees and Toto had long been out chasing birds around him and squirrels. She sat up and looked around her. There was the Scarecrow, still standing patiently in his corner, waiting for her.\n"
"\n"
"\"We must go and search for water,\" she said to him.\n"
"\n"
"\"Why do you want water?\" he asked.\n"
"\n"
"\"To wash my face clean after the dust of the road, and to drink, so the dry bread will not stick in my throat.\"\n"
"\n"
"\"It must be inconvenient to be made of flesh,\" said the Scarecrow thoughtfully, \"for you must sleep, and eat and drink. However, you have brains, and it is worth a lot of bother to be able to think properly.\"\n"
"\n"
"They left the cottage and walked through the trees until they found a little spring of clear water, where Dorothy drank and bathed and ate her breakfast. She saw there was not much bread left in the basket, and the girl was thankful the Scarecrow did not have to eat anything, for there was scarcely enough for herself and Toto for the day.\n"
"\n"
"When she had finished her meal, and was about to go back to the road of yellow brick, she was startled to hear a deep groan near by.\n"
"\n"
"\"What was that?\" she asked timidly.\n"
"\n"
"\"I cannot imagine,\" replied the Scarecrow; \"but we can go and see.\"\n"
"\n"
"Just then another groan reached their ears, and the sound seemed to come from behind them. They turned and walked through the forest a few steps, when Dorothy discovered something shining in a ray of sunshine that fell between the trees. She ran to the place and then stopped short, with a little cry of surprise.\n"
"\n"
"One of the big trees had been partly chopped through, and standing beside it, with an uplifted axe in his hands, was a man made entirely of tin. His head and arms and legs were jointed upon his body, but he stood perfectly motionless, as if he could not stir at all.\n"
"\n"
"Dorothy looked at him in amazement, and so did the Scarecrow, while Toto barked sharply and made a snap at the tin legs, which hurt his teeth.\n"
"\n"
"\"Did you groan?\" asked Dorothy.\n"
"\n"
"\"Yes,\" answered the tin man, \"I did. I've been groaning for more than a year, and no one has ever heard me before or come to help me.\"\n"
"\n"
"\"What can I do for you?\" she inquired softly, for she was moved by the sad voice in which the man spoke.\n"
"\n"
"\"Get an oil-can and oil my joints,\" he answered. \"They are rusted so badly that I cannot move them at all; if I am well oiled I shall soon be all right again. You will find an oil-can on a shelf in my cottage.\"\n"
"\n"
"Dorothy at once ran back to the cottage and found the oil-can, and then she returned and asked anxiously, \"Where are your joints?\"\n"
"\n"
"\"Oil my neck, first,\" replied the Tin Woodman. So she oiled it, and as it was quite badly rusted the Scarecrow took hold of the tin head and moved it gently from side to side until it worked freely, and then the man could turn it himself.\n"
"\n"
"\"Now oil the joints in my arms,\" he said. And Dorothy oiled them and the Scarecrow bent them carefully until they were quite free from rust and as good as new.\n"
"\n"
"The Tin Woodman gave a sigh of satisfaction and lowered his axe, which he leaned against the tree.\n"
"\n"
"\"This is a great comfort,\" he said. \"I have been holding that axe in the air ever since I rusted, and I'm glad to be able to put it down at last. Now, if you will oil the joints of my legs, I shall be all right once more.\"\n"
"\n"
"So they oiled his legs until he could move them freely; and he thanked them again and again for his release, for he seemed a very polite creature, and very grateful.\n"
"\n"
"\"I might have stood there always if you had not come along,\" he said; \"so you have certainly saved my life. How did you happen to be here?\"\n"
"\n"
"\"We are on our way to the Emerald City to see the Great Oz,\" she answered, \"and we stopped at your cottage to pass the night.\"\n"
"\n"
"\"Why do you wish to see Oz?\" he asked.\n"
"\n"
"\"I want him to send me back to Kansas, and the Scarecrow wants him to put a few brains into his head,\" she replied.\n"
"\n"
"The Tin Woodman appeared to think deeply for a moment. Then he said:\n"
"\n"
"\"Do you suppose Oz could give me a heart?\"\n"
"\n"
"\"Why, I guess so,\" Dorothy answered. \"It would be as easy as to give the Scarecrow brains.\"\n"
"\n"
"\"True,\" the Tin Woodman returned. \"So, if you will allow me to join your party, I will also go to the Emerald City and ask Oz to help me.\"\n"
"\n"
"\"Come along,\" said the Scarecrow heartily, and Dorothy added that she would be pleased to have his company. So the Tin Woodman shouldered his axe and they all passed through the forest until they came to the road that was paved with yellow brick.\n"
"\n"
"The Tin Woodman had asked Dorothy to put the oil-can in her basket. \"For,\" he said, \"if I should get caught in the rain, and rust again, I would need the oil-can badly.\"\n"
"\n"
"It was a bit of good luck to have their new comrade join the party, for soon after they had begun their journey again they came to a place where the trees and branches grew so thick over the road that the travelers could not pass. But the Tin Woodman set to work with his axe and chopped so well that soon he cleared a passage for the entire party.\n"
"\n"
"Dorothy was thinking so earnestly as they walked along that she did not notice when the Scarecrow stumbled into a hole and rolled over to the side of the road. Indeed he was obliged to call to her to help him up again.\n"
"\n"
"\"Why didn't you walk around the hole?\" asked the Tin Woodman.\n"
"\n"
"\"I don't know enough,\" replied the Scarecrow cheerfully. \"My head is stuffed with straw, you know, and that is why I am going to Oz to ask him for some brains.\"\n"
"\n"
"\"Oh, I see,\" said the Tin Woodman. \"But, after all, brains are not the best things in the world.\"\n"
"\n"
"\"Have you any?\" inquired the Scarecrow.\n"
"\n"
"\"No, my head is quite empty,\" answered the Woodman. \"But once I had brains, and a heart also; so, having tried them both, I should much rather have a heart.\"\n"
"\n"
"\"And why is that?\" asked the Scarecrow.\n"
"\n"
"\"I will tell you my story, and then you will know.\"\n"
"\n"
"So, while they were walking through the forest, the Tin Woodman told the following story:\n"
"\n"
"\"I was born the son of a woodman who chopped down trees in the forest and sold the wood for a living. When I grew up, I too became a wood-chopper, and after my father died I took care of my old mother as long as she lived. Then I made up my mind that instead of living alone I would marry, so that I might not become lonely.\n"
"\n"
"\"There was one of the Munchkin girls who was so beautiful that I soon grew to love her with all my heart. She, on her part, promised to marry me as soon as I could earn enough money to build a better house for her; so I set to work harder than ever. But the girl lived with an old woman who did not want her to marry anyone, for she was so lazy she wished the girl to remain with her and do the cooking and the housework. So the old woman went to the Wicked Witch of the East, and promised her two sheep and a cow if she would prevent the marriage. Thereupon the Wicked Witch enchanted my axe, and when I was chopping away at my best one day, for I was anxious to get the new house and my wife as soon as possible, the axe slipped all at once and cut off my left leg.\n"
"\n"
"\"This at first seemed a great misfortune, for I knew a one-legged man could not do very well as a wood-chopper. So I went to a tinsmith and had him make me a new leg out of tin. The leg worked very well, once I was used to it. But my action angered the Wicked Witch of the East, for she had promised the old woman I should not marry the pretty Munchkin girl. When I began chopping again, my axe slipped and cut off my right leg. Again I went to the tinsmith, and again he made me a leg out of tin. After this the enchanted axe cut off my arms, one after the other; but, nothing daunted, I had them replaced with tin ones. The Wicked Witch then made the axe slip and cut off my head, and at first I thought that was the end of me. But the tinsmith happened to come along, and he made me a new head out of tin.\n"
"\n"
"\"I thought I had beaten the Wicked Witch then, and I worked harder than ever; but I little knew how cruel my enemy could be. She thought of a new way to kill my love for the beautiful Munchkin maiden, and made my axe slip again, so that it cut right through my body, splitting me into two halves. Once more the tinsmith came to my help and made me a body of tin, fastening my tin arms and legs and head to it, by means of joints, so that I could move around as well as ever. But, alas! I had now no heart, so that I lost all my love for the Munchkin girl, and did not care whether I married her or not. I suppose she is still living with the old woman, waiting for me to come after her.\n"
"\n"
"\"My body shone so brightly in the sun that I felt very proud of it and it did not matter now if my axe slipped, for it could not cut me. There was only one danger—that my joints would rust; but I kept an oil-can in my cottage and took care to oil myself whenever I needed it. However, there came a day when I forgot to do this, and, being caught in a rainstorm, before I thought of the danger my joints had rusted, and I was left to stand in the woods until you came to help me. It was a terrible thing to undergo, but during the year I stood there I had time to think that the greatest loss I had known was the loss of my heart. While I was in love I was the happiest man on earth; but no one can love who has not a heart, and so I am resolved to ask Oz to give me one. If he does, I will go back to the Munchkin maiden and marry her.\"\n"
"\n"
"Both Dorothy and the Scarecrow had been greatly interested in the story of the Tin Woodman, and now they knew why he was so anxious to get a new heart.\n"
"\n"
"\"All the same,\" said the Scarecrow, \"I shall ask for brains instead of a heart; for a fool would not know what to do with a heart if he had one.\"\n"
"\n"
"\"I shall take the heart,\" returned the Tin Woodman; \"for brains do not make one happy, and happiness is the best thing in the world.\"\n"
"\n"
"Dorothy did not say anything, for she was puzzled to know which of her two friends was right, and she decided if she could only get back to Kansas and Aunt Em, it did not matter so much whether the Woodman had no brains and the Scarecrow no heart, or each got what he wanted.\n"
"\n"
"What worried her most was that the bread was nearly gone, and another meal for herself and Toto would empty the basket. To be sure, neither the Woodman nor the Scarecrow ever ate anything, but she was not made of tin nor straw, and could not live unless she was fed.\n"
"\n"
; 

void epaper_test()
{
    const char *text = 
        "Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, "\
        "when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into "\
        "electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, "\
        "and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum."\
        "It is a long established fact that a reader will be distracted by the readable content of a page when looking at its layout. The point of using Lorem Ipsum is "\
        "that it has a more-or-less normal distribution of letters, as opposed to using 'Content here, content here', making it look like readable English. Many desktop "\
        "publishing packages and web page editors now use Lorem Ipsum as their default model text, and a search for 'lorem ipsum' will uncover many web sites still in "\
        "their infancy. Various versions have evolved over the years, sometimes by accident, sometimes on purpose (injected humour and the like).";
    epaper.config_pins(&pin_epaper_scl, &pin_epaper_sda, &pin_epaper_dc, &pin_epaper_cs, &pin_epaper_busy, &pin_epaper_rst);
    epaper.init();
    epaper.print_user_id();
    const char *text_pos = book;
    bool update_needed = true;
    int chars_in_this_page = 0;
    int update_counter = 0;
    while (true)
    {
        if (update_needed)
        {
            epaper.clear();
            //epaper.draw_pattern_sw(4);
            //epaper.update(true);
            font.set_cursor(1, 0);
            //font.draw_string("123!!!Hello World from ePaper!");
            chars_in_this_page = text_renderer.draw_text(text_pos);
            epaper.update(update_counter % 8 == 0);
            update_needed = false;
            update_counter++;
            led1.off();
        }


        button1.update();
        button2.update();
        if (button1.is_pressed_event())
        {
            led1.on();
            text_pos += chars_in_this_page;
            update_needed = true;
        }
        else if (button2.is_pressed_event())
        {
            led1.on();
            text_pos -= max(chars_in_this_page, 300);
            if (text_pos < text)
            {
                text_pos = text;
            }
            update_needed = true;
        }
    }
    epaper.print_mem(512);    
}

extern "C" void task_user_input(void *argument)
{
    MX_USB_DEVICE_Init();

    FILE *fuart1 = uart_fopen(&huart1);
    stdout = fuart1;

    FILE *fusb_vcom = usb_vcom_fopen();
    stdout = fusb_vcom;

    HAL_Delay(2000);  // delay for USB reconnect
    printf(BG_BLACK FG_BRIGHT_WHITE VT100_CLEAR_SCREEN VT100_CURSOR_HOME VT100_SHOW_CURSOR);
    printf(ENDL "Hello from %s (FreeRTOS)!" ENDL, MCU_NAME_STR);
#ifdef DEBUG
    printf("DEBUG=1, build time: " __TIME__ ENDL);
#else
    printf("DEBUG=0, build time: " __TIME__ ENDL);
#endif
    printf("SysClk = %ld KHz" ENDL, HAL_RCC_GetSysClockFreq() / 1000);
    init_storage();
    init_shell();  

    epaper_test();

    for(;;)
    {
        shell.run();

        button1.update();
        button2.update();

        if (button1.is_pressed_event())
        {
            printf("Button1 pressed" ENDL);
            led1.on();
        }
        else if (button1.is_released_event())
        {
            printf("Button1 released" ENDL);
            led1.off();
        }

        if (button2.is_pressed_event())
        {
            printf("Button2 pressed" ENDL);
            led2.on();
        }
        else if (button2.is_released_event())
        {
            printf("Button2 released" ENDL);
            led2.off();
        }

        if (g_audio_player)
        {
            g_audio_player->m_input_handler.handle_inputs();
        }
        osDelay(50);
    }      
}
