#include "monitor.h"
#include "monitor_platform.h"
#include "monitor_config.h"
#include <stdlib.h>

enum monitor_command_status command_tc08(struct monitor_runtime *runtime,
                                         char **state) {
    (void)state;
    if (!runtime) {
        return MONITOR_COMMAND_ERROR;
    }
    monitor_console_puts("TC08 DECtape controller");
    monitor_console_puts("  device code      : 076x/077x");
    const char *tc08_image0 = getenv("TC08_IMAGE0");
    const char *tc08_image1 = getenv("TC08_IMAGE1");
    monitor_console_printf("  unit 0 (RO)      : %s\n",
                           tc08_image0 ? tc08_image0 : "media/boot-tc08.tu56");
    monitor_console_printf("  unit 1 (RW)      : %s\n",
                           tc08_image1 ? tc08_image1 : "magtape/tc08-unit1.tu56");
    monitor_console_puts("  status           : ready (minimal model)");
    return MONITOR_COMMAND_OK;
}
