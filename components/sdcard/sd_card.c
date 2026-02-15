#include "sd_card.h"
#include "lvgl.h"
static const char *TAG = "example";

sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;

sdmmc_host_t host = SDSPI_HOST_DEFAULT();

static esp_err_t s_example_write_file(const char *path, char *data)
{
    // Opening file
    ESP_LOGW(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        // Failed to open file for writing
        ESP_LOGW(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    // Write data to file
    fprintf(f, data);
    fclose(f);
    // File written
    ESP_LOGW(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{
    // Reading file
    ESP_LOGW(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        // Failed to open file for reading
        ESP_LOGW(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    // Read a line from the file
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    // Read from file
    ESP_LOGW(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

esp_err_t waveshare_sd_card_init(esp_err_t i2c_handle)
{
    esp_err_t ret;

    // Control CH422G to pull down the CS pin of the SD
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    write_buf = 0x0E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = false, // If mount fails, format the card
#else
        .format_if_mount_failed = false, // If mount fails, do not format card
#endif
        .max_files = 5,                   // Maximum number of files
        .allocation_unit_size = 16 * 1024 // Set allocation unit size
    };

    // Initializing SD card
    ESP_LOGW(TAG, "Initializing SD card");

    // Configure SPI bus for SD card configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI, // Set MOSI pin
        .miso_io_num = PIN_NUM_MISO, // Set MISO pin
        .sclk_io_num = PIN_NUM_CLK,  // Set SCLK pin
        .quadwp_io_num = -1,         // Not used
        .quadhd_io_num = -1,         // Not used
        .max_transfer_sz = 4000,     // Maximum transfer size
    };
    // Initialize SPI bus
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        // Failed to initialize bus
        ESP_LOGW(TAG, "Failed to initialize bus.");
        return ESP_FAIL;
    }

    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS; // Set CS pin
    slot_config.host_id = host.slot;  // Set host ID

    // Mounting filesystem
    ESP_LOGW(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            // Failed to mount filesystem
            ESP_LOGW(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            // Failed to initialize the card
            ESP_LOGW(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    // Filesystem mounted
    ESP_LOGW(TAG, "Filesystem mounted");
    return ESP_OK;
}

esp_err_t waveshare_sd_card_test()
{
    esp_err_t ret;

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files

    // First create a file
    const char *file_hello = MOUNT_POINT "/hello.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
    // Write data to file
    ret = s_example_write_file(file_hello, data);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    const char *file_foo = MOUNT_POINT "/foo.txt";

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0)
    {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    ESP_LOGW(TAG, "Renaming file %s to %sv", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0)
    {
        // Rename failed
        ESP_LOGW(TAG, "Rename failed");
        return ESP_FAIL;
    }

    // Read renamed file
    ret = s_example_read_file(file_foo);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

       // Format FATFS
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        return;
    }

    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        return;
    } else {
        ESP_LOGI(TAG, "file doesn't exist, formatting done");
    }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

    // Create a new file "nihao.txt" after formatting
    const char *file_nihao = MOUNT_POINT "/nihao.txt";
    memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);                                     // Clear the data buffer
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name); // Writing data
    ret = s_example_write_file(file_nihao, data);                               // Writing data to a file
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Open and read the newly created file
    ret = s_example_read_file(file_nihao);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGW(TAG, "Card unmounted");

    // Deinitialize the SPI bus after all devices are removed
    spi_bus_free(host.slot);
    return ESP_OK;
}

FRESULT list_dir (const char *path)
{
    FRESULT res;
    FF_DIR dir;
    FILINFO fno;
    int nfile, ndir;

    res = f_opendir(&dir, path);                   /* Open the directory */
    if (res == FR_OK) {
        nfile = ndir = 0;
        for (;;) {
            res = f_readdir(&dir, &fno);           /* Read a directory item */
            if (fno.fname[0] == 0) break;          /* Error or end of dir */
            if (fno.fattrib & AM_DIR) {            /* It is a directory */
                printf("   <DIR>   %s\n", fno.fname);
                ndir++;
            } else {                               /* It is a file */
                printf("%10lu %s\n", fno.fsize, fno.fname);
                nfile++;
            }
        }
        f_closedir(&dir);
        printf("%d dirs, %d files.\n", ndir, nfile);
    } else {
        printf("Failed to open %s  (%u)\n", path, res);
    }
    return res;
}

FRESULT scan_files (char* path)
{
    FRESULT res;
    FF_DIR dir;
    UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);                   /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);           /* Read a directory item */
            if (fno.fname[0] == 0) break;          /* Break on error or end of dir */
            if (fno.fattrib & AM_DIR) {            /* The item is a directory */
                i = strlen(path);
                sprintf(&path[i], "/%s", fno.fname);
                res = scan_files(path);            /* Enter the directory */
                if (res != FR_OK) break;
                path[i] = 0;
            } else {                               /* The item is a file. */
                printf("%s/%s\n", path, fno.fname);
            }
        }
        f_closedir(&dir);
    }
    return res;
}


int run_main_fs (void)
{
    FATFS fs;
    FRESULT res;
    char buff[256];


    res = f_mount(&fs, "", 1);
    if (res == FR_OK) {
        strcpy(buff, "/");
        res = scan_files(buff);
    }

    return res;
}

void test_lvgl_sd_read(void)
{
    lv_fs_dir_t dir;
    lv_fs_res_t res;
    
    // Open directory using the 'S' drive letter you set in menuconfig
    res = lv_fs_dir_open(&dir, "S:/"); 
    if(res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "LVGL FS could not open directory (Res: %d)", res);
        return;
    }

    char fn[256];
    while(lv_fs_dir_read(&dir, fn) == LV_FS_RES_OK && strlen(fn) > 0) {
        printf("Found file: %s\n", fn);
    }

    lv_fs_dir_close(&dir);
}