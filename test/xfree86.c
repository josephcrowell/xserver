/**
 * Copyright © 2011 Red Hat, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice (including the next
 *  paragraph) shall be included in all copies or substantial portions of the
 *  Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

/* Test relies on assert() */
#undef NDEBUG

#include <dix-config.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "xf86.h"
#include "xf86Parser_priv.h"

#ifdef XSERVER_PLATFORM_BUS
#include "xf86Config.h"
#include "xf86Priv.h"
#include "xf86platformBus_priv.h"
#include "config/hotplug_priv.h"
#endif
#if defined(XSERVER_PLATFORM_BUS) && defined(XSERVER_LIBPCIACCESS)
#include "xf86Bus.h"
#endif

#include "tests-common.h"

static void
xfree86_option_list_duplicate(void)
{
    XF86OptionPtr options;
    XF86OptionPtr duplicate;
    const char *o1 = "foo", *o2 = "bar", *v1 = "one", *v2 = "two";
    const char *o_null = "NULL";
    char *val1, *val2;
    XF86OptionPtr a, b;

    duplicate = xf86OptionListDuplicate(NULL);
    assert(!duplicate);

    options = xf86AddNewOption(NULL, o1, v1);
    assert(options);
    options = xf86AddNewOption(options, o2, v2);
    assert(options);
    options = xf86AddNewOption(options, o_null, NULL);
    assert(options);

    duplicate = xf86OptionListDuplicate(options);
    assert(duplicate);

    val1 = xf86CheckStrOption(options, o1, "1");
    val2 = xf86CheckStrOption(duplicate, o1, "2");

    assert(strcmp(val1, v1) == 0);
    assert(strcmp(val1, val2) == 0);
    free(val1);
    free(val2);

    val1 = xf86CheckStrOption(options, o2, "1");
    val2 = xf86CheckStrOption(duplicate, o2, "2");

    assert(strcmp(val1, v2) == 0);
    assert(strcmp(val1, val2) == 0);
    free(val1);
    free(val2);

    a = xf86FindOption(options, o_null);
    b = xf86FindOption(duplicate, o_null);
    assert(a);
    assert(b);

    xf86OptionListFree(duplicate);
    xf86OptionListFree(options);
}

static void
xfree86_add_comment(void)
{
    char *current = NULL;
    const char *comment;
    char compare[1024] = { 0 };

    comment = "# foo";
    current = xf86addComment(current, comment);
    strcpy(compare, comment);
    strcat(compare, "\n");

    assert(!strcmp(current, compare));

    /* this used to overflow */
    strcpy(current, "\n");
    comment = "foobar\n";
    current = xf86addComment(current, comment);
    strcpy(compare, "\n#");
    strcat(compare, comment);
    assert(!strcmp(current, compare));

    free(current);
}

/*
 * A platform device without a bus (e.g. vkms) has no busid. Scanning the
 * PCI devices, as GPU hot-plug does, must skip it and not crash.
 */
static void
xfree86_platform_scan_no_busid(void)
{
#ifdef XSERVER_PLATFORM_BUS
    struct OdevAttributes *attribs = config_odev_allocate_attributes();
    int n = xf86_num_platform_devices;

    attribs->path = strdup("/dev/dri/card9");
    assert(attribs->path);
    assert(attribs->busid == NULL);
    xf86_add_platform_device(attribs, FALSE);

    xf86PlatformScanPciDev();
    assert(xf86_platform_devices[n]->pdev == NULL);

    xf86_remove_platform_device(n);
    assert(xf86_num_platform_devices == n);
#endif
}

/*
 * A GPU screen claims its card without a Device section (numInstances 0).
 * Its slot must still count as claimed. Entities stay in xf86Entities after
 * the test, so everything they point to is static and released at the end.
 */
static void
xfree86_check_slot_gpu_screen(void)
{
#if defined(XSERVER_PLATFORM_BUS) && defined(XSERVER_LIBPCIACCESS)
    static DriverRec testdrv = { .driverName = "testdrv" };
    static DriverRec msdrv = { .driverName = "modesetting" };
    static struct pci_device pci = { .domain = 0x7fff, .bus = 1, .dev = 2, .func = 0 };
    static struct pci_device pci2 = { .domain = 0x7fff, .bus = 1, .dev = 2, .func = 0 };
    static char path7[] = "/dev/dri/card7";
    static char path9[] = "/dev/dri/card9";
    static struct OdevAttributes attr7 = { .path = path7 };
    static struct OdevAttributes attr9 = { .path = path9 };
    static struct xf86_platform_device plat = { .attribs = &attr7, .pdev = &pci };
    static struct xf86_platform_device plat2 = { .attribs = &attr9, .pdev = &pci2 };
    static GDevRec fbms = { .identifier = "fb-ms", .driver = "modesetting" };
    GDevRec ms = { .identifier = "ms", .driver = "modesetting" };
    GDevRec ms8 = { .identifier = "ms8", .driver = "modesetting" };
    GDevRec fbdev = { .identifier = "fbdev", .driver = "fbdev" };
    int e, f;

    ms.options = xf86AddNewOption(NULL, "kmsdev", "/dev/dri/card7");
    ms8.options = xf86AddNewOption(NULL, "kmsdev", "/dev/dri/card8");
    fbms.options = xf86AddNewOption(NULL, "kmsdev", "/dev/dri/card7");

    /* A GPU screen: platform claim, no Device section */
    e = xf86AllocateEntity();
    xf86Entities[e]->driver = &testdrv;
    xf86Entities[e]->bus.type = BUS_PLATFORM;
    xf86Entities[e]->bus.id.plat = &plat;
    assert(xf86Entities[e]->numInstances == 0);

    /* Same PCI address through another node, and as a PCI device */
    assert(!xf86CheckSlot(&plat2, BUS_PLATFORM));
    assert(!xf86CheckPciSlot(&pci));
    /* modesetting fb claim of the same node */
    assert(!xf86CheckSlot(&ms, BUS_NONE));
    /* autoconfigured fbdev must be alone */
    assert(!xf86CheckSlot(&fbdev, BUS_NONE));
    /* another node is free */
    assert(xf86CheckSlot(&ms8, BUS_NONE));

    /* Released: the slot is free again */
    xf86Entities[e]->bus.type = BUS_NONE;
    assert(xf86CheckSlot(&plat2, BUS_PLATFORM));
    assert(xf86CheckSlot(&ms, BUS_NONE));

    /* An fb claim of a node blocks a platform claim of it (#1505) */
    f = xf86AllocateEntity();
    xf86Entities[f]->driver = &msdrv;
    xf86Entities[f]->bus.type = BUS_NONE;
    xf86AddDevToEntity(f, &fbms);
    assert(!xf86CheckSlot(&plat, BUS_PLATFORM));
    xf86RemoveDevFromEntity(f, &fbms);
    assert(xf86Entities[f]->numInstances == 0);
    assert(xf86CheckSlot(&plat, BUS_PLATFORM));

    xf86OptionListFree(fbms.options);
    xf86OptionListFree(ms8.options);
    xf86OptionListFree(ms.options);
#endif
}

/*
 * Entities and drivers keep pointers to platform devices, so a device must
 * keep its address while other devices are added (the list grows) and
 * removed (the list shrinks).
 */
static void
xfree86_platform_device_stable(void)
{
#ifdef XSERVER_PLATFORM_BUS
    enum { NDEV = 64 };
    struct xf86_platform_device *devs[NDEV];
    int base = xf86_num_platform_devices;
    int i;

    for (i = 0; i < NDEV; i++) {
        struct OdevAttributes *attribs = config_odev_allocate_attributes();

        attribs->major = 226;
        attribs->minor = 100 + i;
        xf86_add_platform_device(attribs, FALSE);
        devs[i] = xf86_platform_devices[base + i];
    }

    /* adding grew the list many times; earlier devices didn't move */
    for (i = 0; i < NDEV; i++) {
        assert(xf86_platform_devices[base + i] == devs[i]);
        assert(devs[i]->attribs->minor == 100 + i);
    }

    /* removing one from the middle doesn't move the others either */
    xf86_remove_platform_device(base + NDEV / 2);
    for (i = 0; i < NDEV; i++) {
        if (i == NDEV / 2)
            continue;
        assert(xf86_find_platform_device_by_devnum(226, 100 + i) == devs[i]);
        assert(devs[i]->attribs->minor == 100 + i);
    }

    while (xf86_num_platform_devices > base)
        xf86_remove_platform_device(base);
#endif
}

/* The first matching OutputClass that sets HotplugDriver decides, and later
 * matching classes don't reset it. */
static void
xfree86_hotplug_driver_first_match(void)
{
#ifdef XSERVER_PLATFORM_BUS
    XF86ConfOutputClassRec first = { 0 }, second = { 0 };
    XF86ConfigRec config = { 0 };
    XF86ConfigPtr saved_config = xf86configptr;
    const char *saved_layout = xf86ConfigLayout.id;
    int index = xf86_num_platform_devices;
    const char *hp_driver;

    xorg_list_init(&first.match_driver);
    xorg_list_init(&first.match_layout);
    xorg_list_init(&second.match_driver);
    xorg_list_init(&second.match_layout);
    first.list.next = &second;
    first.option_lst = xf86addNewOption(NULL, strdup("HotplugDriver"), strdup("amdgpu"));
    second.option_lst = xf86addNewOption(NULL, strdup("PrimaryGPU"), strdup("off"));
    config.conf_outputclass_lst = &first;
    xf86configptr = &config;
    xf86ConfigLayout.id = "(implicit)";
    xf86_add_platform_device(config_odev_allocate_attributes(), FALSE);

    /* a later class without HotplugDriver */
    hp_driver = xf86PlatformFindHotplugDriver(index);
    assert(hp_driver && !strcmp(hp_driver, "amdgpu"));

    /* a later class with another HotplugDriver */
    xf86optionListFree(second.option_lst);
    second.option_lst = xf86addNewOption(NULL, strdup("HotplugDriver"), strdup("modesetting"));
    hp_driver = xf86PlatformFindHotplugDriver(index);
    assert(hp_driver && !strcmp(hp_driver, "amdgpu"));

    xf86_remove_platform_device(index);
    xf86optionListFree(first.option_lst);
    xf86optionListFree(second.option_lst);
    xf86configptr = saved_config;
    xf86ConfigLayout.id = saved_layout;
#endif
}

/* An existing config directory named on the command line is used even when
 * it has no config files, and reading it alone gives an empty config. */
static void
xfree86_empty_configdir(void)
{
    char template[] = "configdir-XXXXXX";
    char *dir, *opened;
    XF86ConfigPtr config;

    assert(mkdtemp(template));
    dir = realpath(template, NULL);
    assert(dir);

    xf86initConfigFiles();
    opened = xf86openConfigDirFiles("%A", dir, NULL);
    assert(opened && !strcmp(opened, dir));
    config = xf86readConfigFile();
    assert(config);
    assert(!config->conf_device_lst && !config->conf_screen_lst);
    xf86freeConfig(config);
    xf86closeConfigFile();
    free(opened);

    assert(rmdir(dir) == 0);
    xf86initConfigFiles();
    assert(!xf86openConfigDirFiles("%A", dir, NULL));
    xf86closeConfigFile();
    free(dir);
}

const testfunc_t*
xfree86_test(void)
{
    static const testfunc_t testfuncs[] = {
        xfree86_option_list_duplicate,
        xfree86_add_comment,
        xfree86_platform_scan_no_busid,
        xfree86_check_slot_gpu_screen,
        xfree86_platform_device_stable,
        xfree86_hotplug_driver_first_match,
        xfree86_empty_configdir,
        NULL,
    };
    return testfuncs;
}
