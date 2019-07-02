//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2014-2018 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ******************************************************************************************
//
// \file openSeaChest_Firmware.c command line that performs various firmware/microcode download on a device.

//////////////////////
//  Included files  //
//////////////////////
#include <stdio.h>
#include <ctype.h>
#if defined (__unix__) || defined(__APPLE__) //using this definition because linux and unix compilers both define this. Apple does not define this, which is why it has it's own definition
#include <unistd.h>
#include <getopt.h>
#elif defined (_WIN32)
#include "getopt.h"
#else
#error "OS Not Defined or known"
#endif
#include "common.h"
#include "EULA.h"
#include "openseachest_util_options.h"
#include "operations.h"
#include "drive_info.h"
#include "firmware_download.h"
////////////////////////
//  Global Variables  //
////////////////////////
const char *util_name = "openSeaChest_Firmware";
const char *buildVersion = "2.7.0";

typedef enum _eSeaChestFirmwareExitCodes
{
    SEACHEST_FIRMWARE_EXIT_FIRMWARE_DOWNLOAD_COMPLETE = UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE, //Segmented, full, or deferred + activate completed
    SEACHEST_FIRMWARE_EXIT_DEFERRED_DOWNLOAD_COMPLETED,//deferred update complete. need to power cycle the drive
    SEACHEST_FIRMWARE_EXIT_DEFERRED_CODE_ACTIVATED,//activate command sent to the drive
    SEACHEST_FIRMWARE_EXIT_NO_MATCH_FOUND,//config file only right now
    SEACHEST_FIRMWARE_EXIT_MN_MATCH_FW_MISMATCH,//config file only right now
    SEACHEST_FIRMWARE_EXIT_FIRMWARE_HASH_DOESNT_MATCH,
    SEACHEST_FIRMWARE_EXIT_ALREADY_UP_TO_DATE,
    SEACHEST_FIRMWARE_EXIT_MATCH_FOUND, //used in dry run mode only
    SEACHEST_FIRMWARE_EXIT_MATCH_FOUND_DEFERRED_SUPPORTED, //used in dry run mode only
    //TODO: add more exit codes here! Don't forget to add the string definition below in the help!
    SEACHEST_FIRMWARE_EXIT_MAX_ERROR //don't acutally use this, just for memory allocation below
}eSeaChestFirmwareExitCodes;

////////////////////////////
//  functions to declare  //
////////////////////////////
static void utility_Usage(bool shortUsage);
//-----------------------------------------------------------------------------
//
//  main()
//
//! \brief   Description:  main function that runs and decides what to do based on the passed in args
//
//  Entry:
//!   \param argc =
//!   \param argv =
//!
//  Exit:
//!   \return exitCode = error code returned by the application
//
//-----------------------------------------------------------------------------
int32_t main(int argc, char *argv[])
{
    /////////////////
    //  Variables  //
    /////////////////
    //common utility variables
    int                 ret = SUCCESS;
    eUtilExitCodes      exitCode = UTIL_EXIT_NO_ERROR;
    DEVICE_UTIL_VARS
    DEVICE_INFO_VAR
    SAT_INFO_VAR
    LICENSE_VAR
    ECHO_COMMAND_LINE_VAR
    SCAN_FLAG_VAR
    AGRESSIVE_SCAN_FLAG_VAR
    SHOW_BANNER_VAR
    SHOW_HELP_VAR
    TEST_UNIT_READY_VAR
    SAT_12_BYTE_CDBS_VAR
    FORCE_DRIVE_TYPE_VARS
    ENABLE_LEGACY_PASSTHROUGH_VAR
    //scan output flags
    SCAN_FLAGS_UTIL_VARS
    //tool specific
    DOWNLOAD_FW_VARS
#if defined (_WIN32)
    WIN10_FLEXIBLE_API_USE_VAR
    WIN10_FWDL_FORCE_PT_VAR
#endif
    FIRMWARE_SLOT_VAR
    MODEL_MATCH_VARS
    FW_MATCH_VARS
    NEW_FW_MATCH_VARS
    CHILD_MODEL_MATCH_VARS
    CHILD_FW_MATCH_VARS
    CHILD_NEW_FW_MATCH_VARS
    ONLY_SEAGATE_VAR
    FWDL_SEGMENT_SIZE_VARS
    SHOW_FWDL_SUPPORT_VAR
    ACTIVATE_DEFERRED_FW_VAR
    SWITCH_FW_VAR

#if defined (ENABLE_CSMI)
    CSMI_FORCE_VARS
    CSMI_VERBOSE_VAR
#endif

    int8_t  args = 0;
    uint8_t argIndex = 0;
    int32_t optionIndex = 0;
    firmwareUpdateData dlOptions;
    seatimer_t commandTimer;

    //add -- options to this structure DO NOT ADD OPTIONAL ARGUMENTS! Optional arguments are a GNU extension and are not supported in Unix or some compilers- TJE
    struct option longopts[] = {
        //common command line options
        DEVICE_LONG_OPT,
        HELP_LONG_OPT,
        DEVICE_INFO_LONG_OPT,
        SAT_INFO_LONG_OPT,
        USB_CHILD_INFO_LONG_OPT,
        SCAN_LONG_OPT,
        AGRESSIVE_SCAN_LONG_OPT,
        SCAN_FLAGS_LONG_OPT,
        VERSION_LONG_OPT,
        VERBOSE_LONG_OPT,
        QUIET_LONG_OPT,
        LICENSE_LONG_OPT,
        ECHO_COMMAND_LIN_LONG_OPT,
        TEST_UNIT_READY_LONG_OPT,
        SAT_12_BYTE_CDBS_LONG_OPT,
        ONLY_SEAGATE_LONG_OPT,
        MODEL_MATCH_LONG_OPT,
        FW_MATCH_LONG_OPT,
        CHILD_MODEL_MATCH_LONG_OPT,
        CHILD_FW_MATCH_LONG_OPT,
        CHILD_NEW_FW_MATCH_LONG_OPT,
        FORCE_DRIVE_TYPE_LONG_OPTS,
        ENABLE_LEGACY_PASSTHROUGH_LONG_OPT,
#if defined (ENABLE_CSMI)
        CSMI_VERBOSE_LONG_OPT,
        CSMI_FORCE_LONG_OPTS,
#endif
        //tool specific options go here
        DOWNLOAD_FW_LONG_OPT,
        DOWNLOAD_FW_MODE_LONG_OPT,
        NEW_FW_MATCH_LONG_OPT,
        FWDL_SEGMENT_SIZE_LONG_OPT,
        SHOW_FWDL_SUPPORT_LONG_OPT,
        ACTIVATE_DEFERRED_FW_LONG_OPT,
        SWITCH_FW_LONG_OPT,
        FIRMWARE_SLOT_BUFFER_ID_LONG_OPT,
#if defined (_WIN32)
        WIN10_FLEXIBLE_API_USE_LONG_OPT,
        WIN10_FWDL_FORCE_PT_LONG_OPT,
#endif
        LONG_OPT_TERMINATOR
    };

    eVerbosityLevels toolVerbosity = VERBOSITY_DEFAULT;

    ////////////////////////
    //  Argument Parsing  //
    ////////////////////////
    if (argc < 2)
    {
        openseachest_utility_Info(util_name, buildVersion, OPENSEA_TRANSPORT_VERSION);
        utility_Usage(true);
        printf("\n");
        exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
    }
    //get options we know we need
    while (1) //changed to while 1 in order to parse multiple options when longs options are involved
    {
        args = getopt_long(argc, argv, "d:hisSF:Vv:q%:", longopts, &optionIndex);
        if (args == -1)
        {
            break;
        }
        //printf("Parsing arg <%u>\n", args);
        switch (args)
        {
        case 0:
            //parse long options that have no short option and required arguments here
            if (strncmp(longopts[optionIndex].name, DOWNLOAD_FW_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(DOWNLOAD_FW_LONG_OPT_STRING))) == 0)
            {
                DOWNLOAD_FW_FLAG = true;
                sscanf(optarg, "%s", DOWNLOAD_FW_FILENAME_FLAG);
            }
            else if (strncmp(longopts[optionIndex].name, DOWNLOAD_FW_MODE_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(DOWNLOAD_FW_MODE_LONG_OPT_STRING))) == 0)
            {
                USER_SET_DOWNLOAD_MODE = true;
                DOWNLOAD_FW_MODE = DL_FW_SEGMENTED;
                if (strncmp(optarg, "immediate", strlen(optarg)) == 0 || strncmp(optarg, "full", strlen(optarg)) == 0)
                {
                    DOWNLOAD_FW_MODE = DL_FW_FULL;
                }
                else if (strncmp(optarg, "segmented", strlen(optarg)) == 0)
                {
                    DOWNLOAD_FW_MODE = DL_FW_SEGMENTED;
                }
                else if (strncmp(optarg, "deferred", strlen(optarg)) == 0)
                {
                    DOWNLOAD_FW_MODE = DL_FW_DEFERRED;
                }
                else
                {
                    print_Error_In_Cmd_Line_Args(DOWNLOAD_FW_MODE_LONG_OPT_STRING, optarg);
                    exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
                }
            }
            else if (strncmp(longopts[optionIndex].name, MODEL_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(MODEL_MATCH_LONG_OPT_STRING))) == 0)
            {
                MODEL_MATCH_FLAG = true;
                strncpy(MODEL_STRING_FLAG, optarg, M_Min(40, strlen(optarg)));
            }
            else if (strncmp(longopts[optionIndex].name, FW_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(FW_MATCH_LONG_OPT_STRING))) == 0)
            {
                FW_MATCH_FLAG = true;
                strncpy(FW_STRING_FLAG, optarg, M_Min(9, strlen(optarg)));
            }
            else if (strncmp(longopts[optionIndex].name, NEW_FW_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(NEW_FW_MATCH_LONG_OPT_STRING))) == 0)
            {
                NEW_FW_MATCH_FLAG = true;
                strncpy(NEW_FW_STRING_FLAG, optarg, M_Min(9, strlen(optarg)));
            }
            else if (strncmp(longopts[optionIndex].name, CHILD_MODEL_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(CHILD_MODEL_MATCH_LONG_OPT_STRING))) == 0)
            {
                CHILD_MODEL_MATCH_FLAG = true;
                strncpy(CHILD_MODEL_STRING_FLAG, optarg, M_Min(40, strlen(optarg)));
            }
            else if (strncmp(longopts[optionIndex].name, CHILD_FW_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(CHILD_FW_MATCH_LONG_OPT_STRING))) == 0)
            {
                CHILD_FW_MATCH_FLAG = true;
                strncpy(CHILD_FW_STRING_FLAG, optarg, M_Min(9, strlen(optarg)));
            }
            else if (strncmp(longopts[optionIndex].name, CHILD_NEW_FW_MATCH_LONG_OPT_STRING, M_Min(strlen(longopts[optionIndex].name), strlen(CHILD_NEW_FW_MATCH_LONG_OPT_STRING))) == 0)
            {
                CHILD_NEW_FW_MATCH_FLAG = true;
                strncpy(CHILD_NEW_FW_STRING_FLAG, optarg, M_Min(9, strlen(optarg)));
            }
            else if (strcmp(longopts[optionIndex].name, FWDL_SEGMENT_SIZE_LONG_OPT_STRING) == 0)
            {
                FWDL_SEGMENT_SIZE_FROM_USER = true;
                FWDL_SEGMENT_SIZE_FLAG = (uint16_t)atoi(optarg);
            }
            else if (strcmp(longopts[optionIndex].name, FIRMWARE_SLOT_LONG_OPT_STRING) == 0 || strcmp(longopts[optionIndex].name, FIRMWARE_BUFFER_ID_LONG_OPT_STRING) == 0)
            {
                FIRMWARE_SLOT_FLAG = (uint8_t)atoi(optarg);
                if (FIRMWARE_SLOT_FLAG > 7)
                {
                    if (toolVerbosity > VERBOSITY_QUIET)
                    {
                        printf("FirmwareSlot/FwBuffer ID must be between 0 and 7\n");
                    }
                    exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
                }
            }
            break;
        case ':'://missing required argument
            exitCode = UTIL_EXIT_ERROR_IN_COMMAND_LINE;
            switch (optopt)
            {
            case 0:
                //check long options for missing arguments
                break;
            case DEVICE_SHORT_OPT:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("You must specify a device handle\n");
                }
                return UTIL_EXIT_INVALID_DEVICE_HANDLE;
            case VERBOSE_SHORT_OPT:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("You must specify a verbosity level. 0 - 4 are the valid levels\n");
                }
                break;
            case SCAN_FLAGS_SHORT_OPT:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("You must specify which scan options flags you want to use.\n");
                }
                break;
            default:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("Option %c requires an argument\n", optopt);
                }
                utility_Usage(true);
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("\n");
                }
                exit(exitCode);
            }
            break;
        case DEVICE_SHORT_OPT: //device
            if (0 != parse_Device_Handle_Argument(optarg, &RUN_ON_ALL_DRIVES, &USER_PROVIDED_HANDLE, &DEVICE_LIST_COUNT, &HANDLE_LIST))
            {
                //Free any memory allocated so far, then exit.
                free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("\n");
                }
                exit(255);
            }
            break;
        case DEVICE_INFO_SHORT_OPT: //device information
            DEVICE_INFO_FLAG = true;
            break;
        case SCAN_SHORT_OPT: //scan
            SCAN_FLAG = true;
            break;
        case AGRESSIVE_SCAN_SHORT_OPT:
            AGRESSIVE_SCAN_FLAG = true;
            break;
        case VERSION_SHORT_OPT:
            SHOW_BANNER_FLAG = true;
            break;
        case VERBOSE_SHORT_OPT: //verbose
            if (optarg != NULL)
            {
                toolVerbosity = atoi(optarg);
            }
            break;
        case QUIET_SHORT_OPT: //quiet mode
            toolVerbosity = VERBOSITY_QUIET;
            break;
        case SCAN_FLAGS_SHORT_OPT://scan flags
            SCAN_FLAGS_SUBOPT_PARSING;
            break;
        case '?': //unknown option
            printf("%s: Unable to parse %s command line option\nPlease use --%s for more information.\n", util_name, argv[optind - 1], HELP_LONG_OPT_STRING);
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\n");
            }
            exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
        case 'h': //help
            SHOW_HELP_FLAG = true;
            openseachest_utility_Info(util_name, buildVersion, OPENSEA_TRANSPORT_VERSION);
            utility_Usage(false);
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\n");
            }
            exit(UTIL_EXIT_NO_ERROR);
        default:
            break;
        }
    }

    atexit(print_Final_newline);

    if (ECHO_COMMAND_LINE_FLAG)
    {
        uint64_t commandLineIter = 1;//start at 1 as starting at 0 means printing the directory info+ SeaChest.exe (or ./SeaChest)
        for (commandLineIter = 1; commandLineIter < argc; commandLineIter++)
        {
            if (strncmp(argv[commandLineIter], "--echoCommandLine", strlen(argv[commandLineIter])) == 0)
            {
                continue;
            }
            printf("%s ", argv[commandLineIter]);
        }
        printf("\n");
    }

    if (VERBOSITY_QUIET < toolVerbosity)
    {
        openseachest_utility_Info(util_name, buildVersion, OPENSEA_TRANSPORT_VERSION);
    }

    if (SHOW_BANNER_FLAG)
    {
        utility_Full_Version_Info(util_name, buildVersion, OPENSEA_TRANSPORT_MAJOR_VERSION, OPENSEA_TRANSPORT_MINOR_VERSION, OPENSEA_TRANSPORT_PATCH_VERSION, OPENSEA_COMMON_VERSION, OPENSEA_OPERATION_VERSION);
    }

    if (LICENSE_FLAG)
    {
        print_EULA_To_Screen(true, true);
    }

    if (SCAN_FLAG || AGRESSIVE_SCAN_FLAG)
    {
        unsigned int scanControl = DEFAULT_SCAN;
        if(AGRESSIVE_SCAN_FLAG)
        {
            scanControl |= AGRESSIVE_SCAN;
        }
        #if defined (__linux__)
        if (scanSD)
        {
            scanControl |= SD_HANDLES;
        }
        if (scanSDandSG)
        {
            scanControl |= SG_TO_SD;
        }
        #endif
        //set the drive types to show (if none are set, the lower level code assumes we need to show everything)
        if (scanATA)
        {
            scanControl |= ATA_DRIVES;
        }
        if (scanUSB)
        {
            scanControl |= USB_DRIVES;
        }
        if (scanSCSI)
        {
            scanControl |= SCSI_DRIVES;
        }
        if (scanNVMe)
        {
            scanControl |= NVME_DRIVES;
        }
        if (scanRAID)
        {
            scanControl |= RAID_DRIVES;
        }
        //set the interface types to show (if none are set, the lower level code assumes we need to show everything)
        if (scanInterfaceATA)
        {
            scanControl |= IDE_INTERFACE_DRIVES;
        }
        if (scanInterfaceUSB)
        {
            scanControl |= USB_INTERFACE_DRIVES;
        }
        if (scanInterfaceSCSI)
        {
            scanControl |= SCSI_INTERFACE_DRIVES;
        }
        if (scanInterfaceNVMe)
        {
            scanControl |= NVME_INTERFACE_DRIVES;
        }
        if (SAT_12_BYTE_CDBS_FLAG)
        {
            scanControl |= SAT_12_BYTE;
        }
#if defined (ENABLE_CSMI)
        if (scanIgnoreCSMI)
        {
            scanControl |= IGNORE_CSMI;
        }
        if (scanAllowDuplicateDevices)
        {
            scanControl |= ALLOW_DUPLICATE_DEVICE;
        }
#endif
        if (ONLY_SEAGATE_FLAG)
        {
            scanControl |= SCAN_SEAGATE_ONLY;
        }
        scan_And_Print_Devs(scanControl, NULL, toolVerbosity);
    }
    // Add to this if list anything that is suppose to be independent.
    // e.g. you can't say enumerate & then pull logs in the same command line.
    // SIMPLE IS BEAUTIFUL
    if (SCAN_FLAG || AGRESSIVE_SCAN_FLAG || SHOW_BANNER_FLAG || LICENSE_FLAG || SHOW_HELP_FLAG)
    {
        free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
        exit(UTIL_EXIT_NO_ERROR);
    }

    //print out errors for unknown arguments for remaining args that haven't been processed yet
    for (argIndex = optind; argIndex < argc; argIndex++)
    {
        if (VERBOSITY_QUIET < toolVerbosity)
        {
            printf("Invalid argument: %s\n", argv[argIndex]);
        }
    }
    if (RUN_ON_ALL_DRIVES && !USER_PROVIDED_HANDLE)
    {
        uint64_t flags = 0;
#if defined (ENABLE_CSMI)
        flags |= GET_DEVICE_FUNCS_IGNORE_CSMI;
#endif
        if (SUCCESS != get_Device_Count(&DEVICE_LIST_COUNT, flags))
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("Unable to get number of devices\n");
            }
            exit(UTIL_EXIT_OPERATION_FAILURE);
        }
    }
    else if (DEVICE_LIST_COUNT == 0)
    {
        if (VERBOSITY_QUIET < toolVerbosity)
        {
            printf("You must specify one or more target devices with the --%s option to run this command.\n", DEVICE_LONG_OPT_STRING);
            utility_Usage(true);
            printf("Use -h option for detailed description\n\n");
        }
        exit(UTIL_EXIT_INVALID_DEVICE_HANDLE);
    }

    if ((FORCE_SCSI_FLAG && FORCE_ATA_FLAG)
        || (FORCE_ATA_PIO_FLAG && FORCE_ATA_DMA_FLAG && FORCE_ATA_UDMA_FLAG)
        || (FORCE_ATA_PIO_FLAG && FORCE_ATA_DMA_FLAG)
        || (FORCE_ATA_PIO_FLAG && FORCE_ATA_UDMA_FLAG)
        || (FORCE_ATA_DMA_FLAG && FORCE_ATA_UDMA_FLAG)
        || (FORCE_SCSI_FLAG && (FORCE_ATA_PIO_FLAG || FORCE_ATA_DMA_FLAG || FORCE_ATA_UDMA_FLAG))//We may need to remove this. At least when software SAT gets used. (currently only Windows ATA passthrough and FreeBSD ATA passthrough)
        )
    {
        printf("\nError: Only one force flag can be used at a time.\n");
        free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
        exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
    }
    //need to make sure this is set when we are asked for SAT Info
    if (SAT_INFO_FLAG)
    {
        DEVICE_INFO_FLAG = goTrue;
    }
    //check that we were given at least one test to perform...if not, show the help and exit
    if (!(DEVICE_INFO_FLAG
        || TEST_UNIT_READY_FLAG
        //check for other tool specific options here
        || DOWNLOAD_FW_FLAG
        || SHOW_FWDL_SUPPORT_INFO_FLAG
        || ACTIVATE_DEFERRED_FW_FLAG
        || SWITCH_FW_FLAG
        ))
    {
        utility_Usage(true);
        free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
        exit(UTIL_EXIT_ERROR_IN_COMMAND_LINE);
    }

    uint64_t flags = 0;
    DEVICE_LIST = (tDevice*)calloc(DEVICE_LIST_COUNT * sizeof(tDevice), sizeof(tDevice));
    if (!DEVICE_LIST)
    {
        if (VERBOSITY_QUIET < toolVerbosity)
        {
            printf("Unable to allocate memory\n");
        }
        free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
        exit(UTIL_EXIT_OPERATION_FAILURE);
    }
    versionBlock version;
    memset(&version, 0, sizeof(versionBlock));
    version.version = DEVICE_BLOCK_VERSION;
    version.size = sizeof(tDevice);

    if (TEST_UNIT_READY_FLAG)
    {
        flags = DO_NOT_WAKE_DRIVE;
    }

    //set flags that can be passed down in get device regarding forcing specific ATA modes.
    if (FORCE_ATA_PIO_FLAG)
    {
        flags |= FORCE_ATA_PIO_ONLY;
    }

    if (FORCE_ATA_DMA_FLAG)
    {
        flags |= FORCE_ATA_DMA_SAT_MODE;
    }

    if (FORCE_ATA_UDMA_FLAG)
    {
        flags |= FORCE_ATA_UDMA_SAT_MODE;
    }

#if defined (ENABLE_CSMI)
    if (CSMI_VERBOSE_FLAG)
    {
        flags |= CSMI_FLAG_VERBOSE;//This may be temporary....or it may last longer, but for now this highest bit is what we'll set for this option...
    }
    if (CSMI_FORCE_USE_PORT_FLAG)
    {
        flags |= CSMI_FLAG_USE_PORT;
    }
    if (CSMI_FORCE_IGNORE_PORT_FLAG)
    {
        flags |= CSMI_FLAG_IGNORE_PORT;
    }
#endif

    if (RUN_ON_ALL_DRIVES && !USER_PROVIDED_HANDLE)
    {
        //TODO? check for this flag ENABLE_LEGACY_PASSTHROUGH_FLAG. Not sure it is needed here and may not be desirable.
#if defined (ENABLE_CSMI)
        flags |= GET_DEVICE_FUNCS_IGNORE_CSMI;//TODO: Remove this flag so that CSMI devices can be part of running on all drives. This is not allowed now because of issues with running the same operation on the same drive with both PD? and SCSI?:? handles.
#endif
        for (uint32_t devi = 0; devi < DEVICE_LIST_COUNT; ++devi)
        {
            DEVICE_LIST[devi].deviceVerbosity = toolVerbosity;
        }
        ret = get_Device_List(DEVICE_LIST, DEVICE_LIST_COUNT * sizeof(tDevice), version, flags);
        if (SUCCESS != ret)
        {
            if (ret == WARN_NOT_ALL_DEVICES_ENUMERATED)
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("WARN: Not all devices enumerated correctly\n");
                }
            }
            else
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("Unable to get device list\n");
                }
                exit(UTIL_EXIT_OPERATION_FAILURE);
            }
        }
    }
    else
    {
        /*need to go through the handle list and attempt to open each handle.*/
        for (uint16_t handleIter = 0; handleIter < DEVICE_LIST_COUNT; ++handleIter)
        {
            /*Initializing is necessary*/
            deviceList[handleIter].sanity.size = sizeof(tDevice);
            deviceList[handleIter].sanity.version = DEVICE_BLOCK_VERSION;
#if !defined(_WIN32)
            deviceList[handleIter].os_info.fd = -1;
#if defined(VMK_CROSS_COMP)
            deviceList[handleIter].os_info.nvmeFd = NULL;
#endif
#else
            deviceList[handleIter].os_info.fd = INVALID_HANDLE_VALUE;
#endif
            deviceList[handleIter].dFlags = flags;

            deviceList[handleIter].deviceVerbosity = toolVerbosity;

            if (ENABLE_LEGACY_PASSTHROUGH_FLAG)
            {
                deviceList[handleIter].drive_info.ata_Options.enableLegacyPassthroughDetectionThroughTrialAndError = true;
            }

            /*get the device for the specified handle*/
#if defined(_DEBUG)
            printf("Attempting to open handle \"%s\"\n", HANDLE_LIST[handleIter]);
#endif
            ret = get_Device(HANDLE_LIST[handleIter], &deviceList[handleIter]);
#if !defined(_WIN32)
#if !defined(VMK_CROSS_COMP)
            if ((deviceList[handleIter].os_info.fd < 0) ||
#else
            if (((deviceList[handleIter].os_info.fd < 0) &&
                 (deviceList[handleIter].os_info.nvmeFd == NULL)) ||
#endif
            (ret == FAILURE || ret == PERMISSION_DENIED))
#else
            if ((deviceList[handleIter].os_info.fd == INVALID_HANDLE_VALUE) || (ret == FAILURE || ret == PERMISSION_DENIED))
#endif
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("Error: Could not open handle to %s\n", HANDLE_LIST[handleIter]);
                }
                free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
                exit(UTIL_EXIT_INVALID_DEVICE_HANDLE);
            }
        }
    }
    free_Handle_List(&HANDLE_LIST, DEVICE_LIST_COUNT);
    for (uint32_t deviceIter = 0; deviceIter < DEVICE_LIST_COUNT; ++deviceIter)
    {
        deviceList[deviceIter].deviceVerbosity = toolVerbosity;
        if (ONLY_SEAGATE_FLAG)
        {
            if (is_Seagate_Family(&deviceList[deviceIter]) == NON_SEAGATE)
            {
                /*if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive (%s) is not a Seagate drive.\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.product_identification);
                }*/
                continue;
            }
        }

        if (SAT_12_BYTE_CDBS_FLAG)
        {
            //set SAT12 for this device if requested
            deviceList[deviceIter].drive_info.ata_Options.use12ByteSATCDBs = true;
        }

        //check for model number match
        if (MODEL_MATCH_FLAG)
        {
            if (strstr(deviceList[deviceIter].drive_info.product_identification, MODEL_STRING_FLAG) == NULL)
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive (%s) does not match the input model number: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.product_identification, MODEL_STRING_FLAG);
                }
                continue;
            }
        }
        //check for fw already loaded
        if (NEW_FW_MATCH_FLAG)
        {
            if (strcmp(NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision) == 0)
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive already has firmware revision: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.product_revision);
                }
                continue;
            }
        }
        //check for fw match
        if (FW_MATCH_FLAG)
        {
            if (strcmp(FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision))
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive's firmware (%s) does not match the input firmware revision: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.product_revision, FW_STRING_FLAG);
                }
                continue;
            }
        }

        //check for child model number match
        if (CHILD_MODEL_MATCH_FLAG)
        {
            if (strlen(deviceList[deviceIter].drive_info.bridge_info.childDriveMN) == 0 || strstr(deviceList[deviceIter].drive_info.bridge_info.childDriveMN, CHILD_MODEL_STRING_FLAG) == NULL)
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive (%s) does not match the input child model number: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.bridge_info.childDriveMN, CHILD_MODEL_STRING_FLAG);
                }
                continue;
            }
        }
        //check for child fw already loaded
        if (CHILD_NEW_FW_MATCH_FLAG)
        {
            if (strcmp(CHILD_NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.bridge_info.childDriveFW) == 0)
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This child drive already has firmware revision: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.bridge_info.childDriveFW);
                }
                continue;
            }
        }
        //check for child fw match
        if (CHILD_FW_MATCH_FLAG)
        {
            if (strcmp(CHILD_FW_STRING_FLAG, deviceList[deviceIter].drive_info.bridge_info.childDriveFW))
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("%s - This drive's firmware (%s) does not match the input child firmware revision: %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.bridge_info.childDriveFW, CHILD_FW_STRING_FLAG);
                }
                continue;
            }
        }

        if (FORCE_SCSI_FLAG)
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\tForcing SCSI Drive\n");
            }
            deviceList[deviceIter].drive_info.drive_type = SCSI_DRIVE;
        }

        if (FORCE_ATA_FLAG)
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\tForcing ATA Drive\n");
            }
            deviceList[deviceIter].drive_info.drive_type = ATA_DRIVE;
        }

        if (FORCE_ATA_PIO_FLAG)
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\tAttempting to force ATA Drive commands in PIO Mode\n");
            }
            deviceList[deviceIter].drive_info.ata_Options.dmaSupported = false;
            deviceList[deviceIter].drive_info.ata_Options.dmaMode = ATA_DMA_MODE_NO_DMA;
            deviceList[deviceIter].drive_info.ata_Options.downloadMicrocodeDMASupported = false;
            deviceList[deviceIter].drive_info.ata_Options.readBufferDMASupported = false;
            deviceList[deviceIter].drive_info.ata_Options.readLogWriteLogDMASupported = false;
            deviceList[deviceIter].drive_info.ata_Options.writeBufferDMASupported = false;
        }

        if (FORCE_ATA_DMA_FLAG)
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\tAttempting to force ATA Drive commands in DMA Mode\n");
            }
            deviceList[deviceIter].drive_info.ata_Options.dmaMode = ATA_DMA_MODE_DMA;
        }

        if (FORCE_ATA_UDMA_FLAG)
        {
            if (VERBOSITY_QUIET < toolVerbosity)
            {
                printf("\tAttempting to force ATA Drive commands in UDMA Mode\n");
            }
            deviceList[deviceIter].drive_info.ata_Options.dmaMode = ATA_DMA_MODE_UDMA;
        }

        if (VERBOSITY_QUIET < toolVerbosity)
        {
            printf("\n%s - %s - %s - %s\n", deviceList[deviceIter].os_info.name, deviceList[deviceIter].drive_info.product_identification, deviceList[deviceIter].drive_info.serialNumber, print_drive_type(&deviceList[deviceIter]));
        }

#if defined (_WIN32) && WINVER >= SEA_WIN32_WINNT_WIN10
        if (WIN10_FLEXIBLE_API_USE_FLAG)
        {
            deviceList[deviceIter].os_info.fwdlIOsupport.allowFlexibleUseOfAPI = true;
        }

        if (WIN10_FWDL_FORCE_PT_FLAG)
        {
            deviceList[deviceIter].os_info.fwdlIOsupport.fwdlIOSupported = false;//turn off the Win10 API support to force passthrough mode.
        }

#endif

        //now start looking at what operations are going to be performed and kick them off
        if (DEVICE_INFO_FLAG)
        {
            if (SUCCESS != print_Drive_Information(&deviceList[deviceIter], SAT_INFO_FLAG))
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("ERROR: failed to get device information\n");
                }
                exitCode = UTIL_EXIT_OPERATION_FAILURE;
            }
        }

        if (TEST_UNIT_READY_FLAG)
        {
            show_Test_Unit_Ready_Status(&deviceList[deviceIter]);
        }

        if (SHOW_FWDL_SUPPORT_INFO_FLAG)
        {
            supportedDLModes supportedFWDLModes;
            memset(&supportedFWDLModes, 0, sizeof(supportedDLModes));
            switch (get_Supported_FWDL_Modes(&deviceList[deviceIter], &supportedFWDLModes))
            {
            case SUCCESS:
                show_Supported_FWDL_Modes(&deviceList[deviceIter], &supportedFWDLModes);
                break;
            case NOT_SUPPORTED:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("Getting supported download modes not supported on this drive\n");
                }
                exitCode = UTIL_EXIT_OPERATION_NOT_SUPPORTED;
            default:
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("Failed to get supported download modes from the drive\n");
                }
                exitCode = UTIL_EXIT_OPERATION_FAILURE;
            }
        }

        if (DOWNLOAD_FW_FLAG)
        {
            FILE *firmwareFilePtr = NULL;
            bool fileOpenedSuccessfully = true;//assume true in case of activate command
            if (DOWNLOAD_FW_MODE != DL_FW_ACTIVATE)
            {
                //open the file and send the download
                if ((firmwareFilePtr = fopen(DOWNLOAD_FW_FILENAME_FLAG, "rb")) == NULL)
                {
                    fileOpenedSuccessfully = false;
                }
            }
            if (DOWNLOAD_FW_MODE == DL_FW_ACTIVATE)
            {
                //this shouldn't fall into this code path anymore...
                fileOpenedSuccessfully = false;
            }
            if (fileOpenedSuccessfully)
            {
                long firmwareFileSize = get_File_Size(firmwareFilePtr);
                uint8_t *firmwareMem = (uint8_t*)calloc(firmwareFileSize * sizeof(uint8_t), sizeof(uint8_t));
                if (firmwareMem)
                {
                    supportedDLModes supportedFWDLModes;
                    memset(&supportedFWDLModes, 0, sizeof(supportedDLModes));
                    if (SUCCESS == get_Supported_FWDL_Modes(&deviceList[deviceIter], &supportedFWDLModes))
                    {
                        if (!USER_SET_DOWNLOAD_MODE)
                        {
                            //This line is commented out...wait a little longer before letting deferred be a default when supported.
                            /*
                            DOWNLOAD_FW_MODE = supportedFWDLModes.recommendedDownloadMode;
                            /*/
                            if (!supportedFWDLModes.deferred)
                            {
                                DOWNLOAD_FW_MODE = supportedFWDLModes.recommendedDownloadMode;
                            }
                            else
                            {
                                if (supportedFWDLModes.segmented)
                                {
                                    DOWNLOAD_FW_MODE = DL_FW_SEGMENTED;
                                }
                                else
                                {
                                    DOWNLOAD_FW_MODE = DL_FW_FULL;
                                }
                            }
                            //For now, setting deferred download as default for NVMe drives.
                            if (deviceList[deviceIter].drive_info.drive_type == NVME_DRIVE)
                            {
                                DOWNLOAD_FW_MODE = supportedFWDLModes.recommendedDownloadMode;
                            }
                            //*/
                        }
                    }
                    fread(firmwareMem, sizeof(uint8_t), firmwareFileSize, firmwareFilePtr);

                    memset(&dlOptions, 0, sizeof(firmwareUpdateData));
                    memset(&commandTimer, 0, sizeof(seatimer_t));
                    dlOptions.dlMode = DOWNLOAD_FW_MODE;
                    if (FWDL_SEGMENT_SIZE_FROM_USER)
                    {
                        dlOptions.segmentSize = FWDL_SEGMENT_SIZE_FLAG;
                    }
                    else
                    {
                        dlOptions.segmentSize = 0;
                    }
                    dlOptions.firmwareFileMem = firmwareMem;
                    dlOptions.firmwareMemoryLength = firmwareFileSize;
                    dlOptions.firmwareSlot = FIRMWARE_SLOT_FLAG;
                    start_Timer(&commandTimer);
                    ret = firmware_Download(&deviceList[deviceIter], &dlOptions);
                    stop_Timer(&commandTimer);
                    switch (ret)
                    {
                    case SUCCESS:
                        exitCode = (eUtilExitCodes)SEACHEST_FIRMWARE_EXIT_FIRMWARE_DOWNLOAD_COMPLETE;
                        if (VERBOSITY_QUIET < toolVerbosity)
                        {
                            printf("Firmware Download successful\n");
                            printf("Firmware Download time");
                            print_Time(get_Nano_Seconds(commandTimer));
                            printf("Average time/segment ");
                            print_Time(dlOptions.avgSegmentDlTime);
                            if (DOWNLOAD_FW_MODE != DL_FW_DEFERRED)
                            {
                                printf("Activate Time         ");
                                print_Time(dlOptions.activateFWTime);
                            }
                        }
                        if (DOWNLOAD_FW_MODE == DL_FW_DEFERRED)
                        {
                            exitCode = (eUtilExitCodes)SEACHEST_FIRMWARE_EXIT_DEFERRED_DOWNLOAD_COMPLETED;
                            if (VERBOSITY_QUIET < toolVerbosity)
                            {
                                printf("Firmware download complete. Reboot or run the --%s command to finish installing the firmware.\n", ACTIVATE_DEFERRED_FW_LONG_OPT_STRING);
                            }
                        }
                        else if (supportedFWDLModes.seagateDeferredPowerCycleActivate && DOWNLOAD_FW_MODE == DL_FW_SEGMENTED)
                        {
                            exitCode = (eUtilExitCodes)SEACHEST_FIRMWARE_EXIT_DEFERRED_DOWNLOAD_COMPLETED;
                            if (VERBOSITY_QUIET < toolVerbosity)
                            {
                                printf("This drive requires a full power cycle to activate the new code.\n");
                            }
                        }
                        else
                        {
                            fill_Drive_Info_Data(&deviceList[deviceIter]);
                            if (VERBOSITY_QUIET < toolVerbosity)
                            {
                                if (NEW_FW_MATCH_FLAG)
                                {
                                    if (strcmp(NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision) == 0)
                                    {
                                        printf("Successfully validated firmware after download!\n");
                                        printf("New firmware version is %s\n", deviceList[deviceIter].drive_info.product_revision);
                                    }
                                    else
                                    {
                                        printf("Unable to verify firmware after download!, expected %s, but found %s\n", NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision);
                                    }
                                }
                                else
                                {
                                    printf("New firmware version is %s\n", deviceList[deviceIter].drive_info.product_revision);
                                }
                            }
                        }
                        break;
                    case NOT_SUPPORTED:
                        if (VERBOSITY_QUIET < toolVerbosity)
                        {
                            printf("Firmware Download not supported\n");
                        }
                        exitCode = UTIL_EXIT_OPERATION_NOT_SUPPORTED;
                        break;
                    default:
                        if (VERBOSITY_QUIET < toolVerbosity)
                        {
                            printf("Firmware Download failed\n");
                        }
                        exitCode = UTIL_EXIT_OPERATION_FAILURE;
                        break;
                    }
                    safe_Free(firmwareMem);
                }
                else
                {
                    perror("failed to allocate memory");
                    exit(255);
                }
            }
            else
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    perror("fopen");
                    printf("Couldn't open file %s\n", DOWNLOAD_FW_FILENAME_FLAG);
                }
                exitCode = UTIL_EXIT_OPERATION_FAILURE;
            }
        }

        if (ACTIVATE_DEFERRED_FW_FLAG || SWITCH_FW_FLAG)
        {
            supportedDLModes supportedFWDLModes;
            memset(&supportedFWDLModes, 0, sizeof(supportedDLModes));
            get_Supported_FWDL_Modes(&deviceList[deviceIter], &supportedFWDLModes);
            if (supportedFWDLModes.deferred || supportedFWDLModes.scsiInfoPossiblyIncomplete)
            {
                memset(&dlOptions, 0, sizeof(firmwareUpdateData));
                memset(&commandTimer, 0, sizeof(seatimer_t));
                dlOptions.dlMode = DL_FW_ACTIVATE;
                dlOptions.segmentSize = 0;
                dlOptions.firmwareFileMem = NULL;
                dlOptions.firmwareMemoryLength = 0;
                dlOptions.firmwareSlot = FIRMWARE_SLOT_FLAG;
                if (SWITCH_FW_FLAG)
                {
                    dlOptions.existingFirmwareImage = true;
                }
                start_Timer(&commandTimer);
                ret = firmware_Download(&deviceList[deviceIter], &dlOptions);
                stop_Timer(&commandTimer);
                switch (ret)
                {
                case SUCCESS:
                    exitCode = (eUtilExitCodes)SEACHEST_FIRMWARE_EXIT_DEFERRED_CODE_ACTIVATED;
                    if (VERBOSITY_QUIET < toolVerbosity)
                    {
                        printf("Firmware activation successful\n");
                        fill_Drive_Info_Data(&deviceList[deviceIter]);
                        if (NEW_FW_MATCH_FLAG)
                        {
                            if (strcmp(NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision) == 0)
                            {
                                printf("Successfully validated firmware after download!\n");
                                printf("New firmware version is %s\n", deviceList[deviceIter].drive_info.product_revision);
                            }
                            else
                            {
                                printf("Unable to verify firmware after download!, expected %s, but found %s\n", NEW_FW_STRING_FLAG, deviceList[deviceIter].drive_info.product_revision);
                            }
                        }
                        else
                        {
                            printf("New firmware version is %s\n", deviceList[deviceIter].drive_info.product_revision);
                        }
                    }
                    break;
                case NOT_SUPPORTED:
                    if (VERBOSITY_QUIET < toolVerbosity)
                    {
                        if (SWITCH_FW_FLAG && FIRMWARE_SLOT_FLAG == 0)
                        {
                            printf("You must specify a valid slot number when switching firmware images.\n");
                        }
                        else
                        {
                            printf("Firmware activate not supported\n");
                        }
                    }
                    exitCode = UTIL_EXIT_OPERATION_NOT_SUPPORTED;
                    break;
                default:
                    if (VERBOSITY_QUIET < toolVerbosity)
                    {
                        printf("Firmware activation failed\n");
                    }
                    exitCode = UTIL_EXIT_OPERATION_FAILURE;
                    break;
                }
            }
            else
            {
                if (VERBOSITY_QUIET < toolVerbosity)
                {
                    printf("This drive does not support the activate command.\n");
                }
                exitCode = UTIL_EXIT_OPERATION_NOT_SUPPORTED;
            }
        }
    }
    exit(exitCode);
}

//-----------------------------------------------------------------------------
//
//  Utility_usage()
//
//! \brief   Description:  Dump the utility usage information
//
//  Entry:
//!   \param NONE
//!
//  Exit:
//!   \return VOID
//
//-----------------------------------------------------------------------------
void utility_Usage(bool shortUsage)
{
    //everything needs a help option right?
    printf("Usage\n");
    printf("=====\n");
    printf("\t %s [-d %s] {arguments} {options}\n\n", util_name, deviceHandleName);

    printf("Examples\n");
    printf("========\n");
    //example usage
    printf("\t%s --scan\n", util_name);
    printf("\t%s -d %s -i\n", util_name, deviceHandleExample);
    //return codes
    printf("\nReturn codes\n");
    printf("============\n");
    //SEACHEST_FIRMWARE_EXIT_MAX_ERROR - SEACHEST_FIRMWARE_EXIT_FIRMWARE_DOWNLOAD_COMPLETE
    int totalErrorCodes = SEACHEST_FIRMWARE_EXIT_MAX_ERROR - SEACHEST_FIRMWARE_EXIT_FIRMWARE_DOWNLOAD_COMPLETE;
    ptrToolSpecificxitCode seachestFirmwareExitCodes = (ptrToolSpecificxitCode)calloc(totalErrorCodes * sizeof(toolSpecificxitCode), sizeof(toolSpecificxitCode));
    //now set up all the exit codes and their meanings
    if (seachestFirmwareExitCodes)
    {
        for (int exitIter = UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE; exitIter < SEACHEST_FIRMWARE_EXIT_MAX_ERROR; ++exitIter)
        {
            seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCode = exitIter;
            switch (exitIter)
            {
            case SEACHEST_FIRMWARE_EXIT_FIRMWARE_DOWNLOAD_COMPLETE:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Firmware Download Complete", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_DEFERRED_DOWNLOAD_COMPLETED:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Deferred Firmware Download Complete", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_DEFERRED_CODE_ACTIVATED:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Deferred Code Activated", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_NO_MATCH_FOUND:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "No Drive or Firmware match found", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_MN_MATCH_FW_MISMATCH:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Model number matched, but Firmware mismatched", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_FIRMWARE_HASH_DOESNT_MATCH:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Firmware File Hash Error", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_ALREADY_UP_TO_DATE:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Firmware Already up to date", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_MATCH_FOUND:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Firmware Match Found for update", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
            case SEACHEST_FIRMWARE_EXIT_MATCH_FOUND_DEFERRED_SUPPORTED:
                strncpy(seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString, "Firmware Match Found for update - deferred update supported", TOOL_EXIT_CODE_STRING_MAX_LENGTH);
                seachestFirmwareExitCodes[exitIter - UTIL_TOOL_SPECIFIC_STARTING_ERROR_CODE].exitCodeString[TOOL_EXIT_CODE_STRING_MAX_LENGTH - 1] = '\0';
                break;
                //TODO: add more exit codes here!
            default://We shouldn't ever hit the default case!
                break;
            }
        }
    }
    print_SeaChest_Util_Exit_Codes(totalErrorCodes, seachestFirmwareExitCodes, util_name);

    //utility options - alphabetized
    printf("Utility Options\n");
    printf("===============\n");
#if defined (ENABLE_CSMI)
    print_CSMI_Force_Flags_Help(shortUsage);
    print_CSMI_Verbose_Help(shortUsage);
#endif
#if defined (_WIN32)
    print_FWDL_Allow_Flexible_Win10_API_Use_Help(shortUsage);
    print_FWDL_Force_Win_Passthrough_Help(shortUsage);
#endif
    print_Echo_Command_Line_Help(shortUsage);
    print_Enable_Legacy_USB_Passthrough_Help(shortUsage);
    print_Force_ATA_Help(shortUsage);
    print_Force_ATA_DMA_Help(shortUsage);
    print_Force_ATA_PIO_Help(shortUsage);
    print_Force_ATA_UDMA_Help(shortUsage);
    print_Force_SCSI_Help(shortUsage);
    print_Help_Help(shortUsage);
    print_License_Help(shortUsage);
    print_Model_Match_Help(shortUsage);
    print_New_Firmware_Revision_Match_Help(shortUsage);
    print_Firmware_Revision_Match_Help(shortUsage);
    print_Only_Seagate_Help(shortUsage);
    print_Quiet_Help(shortUsage, util_name);
    print_SAT_12_Byte_CDB_Help(shortUsage);
    print_Verbose_Help(shortUsage);
    print_Version_Help(shortUsage, util_name);

    //the test options
    printf("\nUtility Arguments\n");
    printf("=================\n");
    //Common (across utilities) - alphabetized
    print_Device_Help(shortUsage, deviceHandleExample);
    print_Scan_Flags_Help(shortUsage);
    print_Device_Information_Help(shortUsage);
    print_Scan_Help(shortUsage, deviceHandleExample);
    print_Agressive_Scan_Help(shortUsage);
    print_SAT_Info_Help(shortUsage);
    print_Test_Unit_Ready_Help(shortUsage);
    //utility tests/operations go here - alphabetized
    print_Firmware_Activate_Help(shortUsage);
    print_Firmware_Download_Help(shortUsage);
    print_Firmware_Download_Mode_Help(shortUsage);
    print_Firmware_Slot_Buffer_ID_Help(shortUsage);
    print_FWDL_Segment_Size_Help(shortUsage);
    print_show_FWDL_Support_Help(shortUsage);
    print_Firmware_Switch_Help(shortUsage);
}
