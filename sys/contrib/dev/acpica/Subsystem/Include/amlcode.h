/******************************************************************************
 *
 * Name: amlcode.h - Definitions for AML, as included in "definition blocks"
 *                   Declarations and definitions contained herein are derived
 *                   directly from the ACPI specification.
 *       $Revision: 46 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __AMLCODE_H__
#define __AMLCODE_H__


/* primary opcodes */

#define AML_NULL_CHAR               (UINT16) 0x00

#define AML_ZERO_OP                 (UINT16) 0x00
#define AML_ONE_OP                  (UINT16) 0x01
#define AML_UNASSIGNED              (UINT16) 0x02
#define AML_ALIAS_OP                (UINT16) 0x06
#define AML_NAME_OP                 (UINT16) 0x08
#define AML_BYTE_OP                 (UINT16) 0x0a
#define AML_WORD_OP                 (UINT16) 0x0b
#define AML_DWORD_OP                (UINT16) 0x0c
#define AML_STRING_OP               (UINT16) 0x0d
#define AML_QWORD_OP                (UINT16) 0x0e     /* ACPI 2.0 */
#define AML_SCOPE_OP                (UINT16) 0x10
#define AML_BUFFER_OP               (UINT16) 0x11
#define AML_PACKAGE_OP              (UINT16) 0x12
#define AML_VAR_PACKAGE_OP          (UINT16) 0x13     /* ACPI 2.0 */
#define AML_METHOD_OP               (UINT16) 0x14
#define AML_DUAL_NAME_PREFIX        (UINT16) 0x2e
#define AML_MULTI_NAME_PREFIX_OP    (UINT16) 0x2f
#define AML_NAME_CHAR_SUBSEQ        (UINT16) 0x30
#define AML_NAME_CHAR_FIRST         (UINT16) 0x41
#define AML_OP_PREFIX               (UINT16) 0x5b
#define AML_ROOT_PREFIX             (UINT16) 0x5c
#define AML_PARENT_PREFIX           (UINT16) 0x5e
#define AML_LOCAL_OP                (UINT16) 0x60
#define AML_LOCAL0                  (UINT16) 0x60
#define AML_LOCAL1                  (UINT16) 0x61
#define AML_LOCAL2                  (UINT16) 0x62
#define AML_LOCAL3                  (UINT16) 0x63
#define AML_LOCAL4                  (UINT16) 0x64
#define AML_LOCAL5                  (UINT16) 0x65
#define AML_LOCAL6                  (UINT16) 0x66
#define AML_LOCAL7                  (UINT16) 0x67
#define AML_ARG_OP                  (UINT16) 0x68
#define AML_ARG0                    (UINT16) 0x68
#define AML_ARG1                    (UINT16) 0x69
#define AML_ARG2                    (UINT16) 0x6a
#define AML_ARG3                    (UINT16) 0x6b
#define AML_ARG4                    (UINT16) 0x6c
#define AML_ARG5                    (UINT16) 0x6d
#define AML_ARG6                    (UINT16) 0x6e
#define AML_STORE_OP                (UINT16) 0x70
#define AML_REF_OF_OP               (UINT16) 0x71
#define AML_ADD_OP                  (UINT16) 0x72
#define AML_CONCAT_OP               (UINT16) 0x73
#define AML_SUBTRACT_OP             (UINT16) 0x74
#define AML_INCREMENT_OP            (UINT16) 0x75
#define AML_DECREMENT_OP            (UINT16) 0x76
#define AML_MULTIPLY_OP             (UINT16) 0x77
#define AML_DIVIDE_OP               (UINT16) 0x78
#define AML_SHIFT_LEFT_OP           (UINT16) 0x79
#define AML_SHIFT_RIGHT_OP          (UINT16) 0x7a
#define AML_BIT_AND_OP              (UINT16) 0x7b
#define AML_BIT_NAND_OP             (UINT16) 0x7c
#define AML_BIT_OR_OP               (UINT16) 0x7d
#define AML_BIT_NOR_OP              (UINT16) 0x7e
#define AML_BIT_XOR_OP              (UINT16) 0x7f
#define AML_BIT_NOT_OP              (UINT16) 0x80
#define AML_FIND_SET_LEFT_BIT_OP    (UINT16) 0x81
#define AML_FIND_SET_RIGHT_BIT_OP   (UINT16) 0x82
#define AML_DEREF_OF_OP             (UINT16) 0x83
#define AML_CONCAT_RES_OP           (UINT16) 0x84     /* ACPI 2.0 */
#define AML_MOD_OP                  (UINT16) 0x85     /* ACPI 2.0 */
#define AML_NOTIFY_OP               (UINT16) 0x86
#define AML_SIZE_OF_OP              (UINT16) 0x87
#define AML_INDEX_OP                (UINT16) 0x88
#define AML_MATCH_OP                (UINT16) 0x89
#define AML_DWORD_FIELD_OP          (UINT16) 0x8a
#define AML_WORD_FIELD_OP           (UINT16) 0x8b
#define AML_BYTE_FIELD_OP           (UINT16) 0x8c
#define AML_BIT_FIELD_OP            (UINT16) 0x8d
#define AML_TYPE_OP                 (UINT16) 0x8e
#define AML_QWORD_FIELD_OP          (UINT16) 0x8f     /* ACPI 2.0 */
#define AML_LAND_OP                 (UINT16) 0x90
#define AML_LOR_OP                  (UINT16) 0x91
#define AML_LNOT_OP                 (UINT16) 0x92
#define AML_LEQUAL_OP               (UINT16) 0x93
#define AML_LGREATER_OP             (UINT16) 0x94
#define AML_LLESS_OP                (UINT16) 0x95
#define AML_TO_BUFFER_OP            (UINT16) 0x96     /* ACPI 2.0 */
#define AML_TO_DECSTRING_OP         (UINT16) 0x97     /* ACPI 2.0 */
#define AML_TO_HEXSTRING_OP         (UINT16) 0x98     /* ACPI 2.0 */
#define AML_TO_INTEGER_OP           (UINT16) 0x99     /* ACPI 2.0 */
#define AML_TO_STRING_OP            (UINT16) 0x9c     /* ACPI 2.0 */
#define AML_COPY_OP                 (UINT16) 0x9d     /* ACPI 2.0 */
#define AML_MID_OP                  (UINT16) 0x9e     /* ACPI 2.0 */
#define AML_CONTINUE_OP             (UINT16) 0x9f     /* ACPI 2.0 */
#define AML_IF_OP                   (UINT16) 0xa0
#define AML_ELSE_OP                 (UINT16) 0xa1
#define AML_WHILE_OP                (UINT16) 0xa2
#define AML_NOOP_OP                 (UINT16) 0xa3
#define AML_RETURN_OP               (UINT16) 0xa4
#define AML_BREAK_OP                (UINT16) 0xa5
#define AML_BREAK_POINT_OP          (UINT16) 0xcc
#define AML_ONES_OP                 (UINT16) 0xff

/* prefixed opcodes */

#define AML_EXTOP                   (UINT16) 0x005b


#define AML_MUTEX_OP                (UINT16) 0x5b01
#define AML_EVENT_OP                (UINT16) 0x5b02
#define AML_SHIFT_RIGHT_BIT_OP      (UINT16) 0x5b10
#define AML_SHIFT_LEFT_BIT_OP       (UINT16) 0x5b11
#define AML_COND_REF_OF_OP          (UINT16) 0x5b12
#define AML_CREATE_FIELD_OP         (UINT16) 0x5b13
#define AML_LOAD_TABLE_OP           (UINT16) 0x5b1f     /* ACPI 2.0 */
#define AML_LOAD_OP                 (UINT16) 0x5b20
#define AML_STALL_OP                (UINT16) 0x5b21
#define AML_SLEEP_OP                (UINT16) 0x5b22
#define AML_ACQUIRE_OP              (UINT16) 0x5b23
#define AML_SIGNAL_OP               (UINT16) 0x5b24
#define AML_WAIT_OP                 (UINT16) 0x5b25
#define AML_RESET_OP                (UINT16) 0x5b26
#define AML_RELEASE_OP              (UINT16) 0x5b27
#define AML_FROM_BCD_OP             (UINT16) 0x5b28
#define AML_TO_BCD_OP               (UINT16) 0x5b29
#define AML_UNLOAD_OP               (UINT16) 0x5b2a
#define AML_REVISION_OP             (UINT16) 0x5b30
#define AML_DEBUG_OP                (UINT16) 0x5b31
#define AML_FATAL_OP                (UINT16) 0x5b32
#define AML_REGION_OP               (UINT16) 0x5b80
#define AML_DEF_FIELD_OP            (UINT16) 0x5b81
#define AML_DEVICE_OP               (UINT16) 0x5b82
#define AML_PROCESSOR_OP            (UINT16) 0x5b83
#define AML_POWER_RES_OP            (UINT16) 0x5b84
#define AML_THERMAL_ZONE_OP         (UINT16) 0x5b85
#define AML_INDEX_FIELD_OP          (UINT16) 0x5b86
#define AML_BANK_FIELD_OP           (UINT16) 0x5b87
#define AML_DATA_REGION_OP          (UINT16) 0x5b88     /* ACPI 2.0 */


/* Bogus opcodes (they are actually two separate opcodes) */

#define AML_LGREATEREQUAL_OP        (UINT16) 0x9295
#define AML_LLESSEQUAL_OP           (UINT16) 0x9294
#define AML_LNOTEQUAL_OP            (UINT16) 0x9293


/*
 * Internal opcodes
 * Use only "Unknown" AML opcodes, don't attempt to use
 * any valid ACPI ASCII values (A-Z, 0-9, '-')
 */

#define AML_NAMEPATH_OP             (UINT16) 0x002d
#define AML_NAMEDFIELD_OP           (UINT16) 0x0030
#define AML_RESERVEDFIELD_OP        (UINT16) 0x0031
#define AML_ACCESSFIELD_OP          (UINT16) 0x0032
#define AML_BYTELIST_OP             (UINT16) 0x0033
#define AML_STATICSTRING_OP         (UINT16) 0x0034
#define AML_METHODCALL_OP           (UINT16) 0x0035
#define AML_RETURN_VALUE_OP         (UINT16) 0x0036


#define ARG_NONE                    0x0

/*
 * Argument types for the AML Parser
 * Each field in the ArgTypes UINT32 is 5 bits, allowing for a maximum of 6 arguments.
 * There can be up to 31 unique argument types
 */

#define ARGP_BYTEDATA               0x01
#define ARGP_BYTELIST               0x02
#define ARGP_CHARLIST               0x03
#define ARGP_DATAOBJ                0x04
#define ARGP_DATAOBJLIST            0x05
#define ARGP_DWORDDATA              0x06
#define ARGP_FIELDLIST              0x07
#define ARGP_NAME                   0x08
#define ARGP_NAMESTRING             0x09
#define ARGP_OBJLIST                0x0A
#define ARGP_PKGLENGTH              0x0B
#define ARGP_SUPERNAME              0x0C
#define ARGP_TARGET                 0x0D
#define ARGP_TERMARG                0x0E
#define ARGP_TERMLIST               0x0F
#define ARGP_WORDDATA               0x10
#define ARGP_QWORDDATA              0x11
#define ARGP_SIMPLENAME             0x12

/*
 * Resolved argument types for the AML Interpreter
 * Each field in the ArgTypes UINT32 is 5 bits, allowing for a maximum of 6 arguments.
 * There can be up to 31 unique argument types (0 is end-of-arg-list indicator)
 */

/* "Standard" ACPI types are 1-15 (0x0F) */

#define ARGI_INTEGER                 ACPI_TYPE_INTEGER        /* 1 */
#define ARGI_STRING                 ACPI_TYPE_STRING        /* 2 */
#define ARGI_BUFFER                 ACPI_TYPE_BUFFER        /* 3 */
#define ARGI_PACKAGE                ACPI_TYPE_PACKAGE       /* 4 */
#define ARGI_EVENT                  ACPI_TYPE_EVENT
#define ARGI_MUTEX                  ACPI_TYPE_MUTEX
#define ARGI_REGION                 ACPI_TYPE_REGION
#define ARGI_DDBHANDLE              ACPI_TYPE_DDB_HANDLE

/* Custom types are 0x10 through 0x1F */

#define ARGI_IF                     0x10
#define ARGI_ANYOBJECT              0x11
#define ARGI_ANYTYPE                0x12
#define ARGI_COMPUTEDATA            0x13     /* Buffer, String, or Integer */
#define ARGI_DATAOBJECT             0x14     /* Buffer, string, package or reference to a Node - Used only by SizeOf operator*/
#define ARGI_COMPLEXOBJ             0x15     /* Buffer or package */
#define ARGI_INTEGER_REF            0x16
#define ARGI_OBJECT_REF             0x17
#define ARGI_DEVICE_REF             0x18
#define ARGI_REFERENCE              0x19
#define ARGI_TARGETREF              0x1A     /* Target, subject to implicit conversion */
#define ARGI_FIXED_TARGET           0x1B     /* Target, no implicit conversion */
#define ARGI_SIMPLE_TARGET          0x1C     /* Name, Local, Arg -- no implicit conversion */
#define ARGI_BUFFERSTRING           0x1D

#define ARGI_INVALID_OPCODE         0xFFFFFFFF


/*
 * hash offsets
 */
#define AML_EXTOP_HASH_OFFSET       22
#define AML_LNOT_HASH_OFFSET        19


/*
 * opcode groups and types
 */

#define OPGRP_NAMED                 0x01
#define OPGRP_FIELD                 0x02
#define OPGRP_BYTELIST              0x04

#define OPTYPE_UNDEFINED            0


#define OPTYPE_LITERAL              1
#define OPTYPE_CONSTANT             2
#define OPTYPE_METHOD_ARGUMENT      3
#define OPTYPE_LOCAL_VARIABLE       4
#define OPTYPE_DATA_TERM            5

/* Type 1 opcodes */

#define OPTYPE_MONADIC1             6
#define OPTYPE_DYADIC1              7


/* Type 2 opcodes */

#define OPTYPE_MONADIC2             8
#define OPTYPE_MONADIC2R            9
#define OPTYPE_DYADIC2              10
#define OPTYPE_DYADIC2R             11
#define OPTYPE_DYADIC2S             12
#define OPTYPE_INDEX                13
#define OPTYPE_MATCH                14

/* Generic for an op that returns a value */

#define OPTYPE_METHOD_CALL          15


/* Misc */

#define OPTYPE_CREATE_FIELD         16
#define OPTYPE_FATAL                17
#define OPTYPE_CONTROL              18
#define OPTYPE_RECONFIGURATION      19
#define OPTYPE_NAMED_OBJECT         20
#define OPTYPE_RETURN               21

#define OPTYPE_BOGUS                22


/* Predefined Operation Region SpaceIDs */

typedef enum
{
    REGION_MEMORY               = 0,
    REGION_IO,
    REGION_PCI_CONFIG,
    REGION_EC,
    REGION_SMBUS,
    REGION_CMOS,
    REGION_PCI_BAR

} AML_REGION_TYPES;


/* Comparison operation codes for MatchOp operator */

typedef enum
{
    MATCH_MTR                   = 0,
    MATCH_MEQ                   = 1,
    MATCH_MLE                   = 2,
    MATCH_MLT                   = 3,
    MATCH_MGE                   = 4,
    MATCH_MGT                   = 5

} AML_MATCH_OPERATOR;

#define MAX_MATCH_OPERATOR      5


/* Field Access Types */

#define ACCESS_TYPE_MASK        0x0f
#define ACCESS_TYPE_SHIFT       0

typedef enum
{
    ACCESS_ANY_ACC              = 0,
    ACCESS_BYTE_ACC             = 1,
    ACCESS_WORD_ACC             = 2,
    ACCESS_DWORD_ACC            = 3,
    ACCESS_BLOCK_ACC            = 4,
    ACCESS_SMBSEND_RECV_ACC     = 5,
    ACCESS_SMBQUICK_ACC         = 6

} AML_ACCESS_TYPE;


/* Field Lock Rules */

#define LOCK_RULE_MASK          0x10
#define LOCK_RULE_SHIFT         4

typedef enum
{
    GLOCK_NEVER_LOCK            = 0,
    GLOCK_ALWAYS_LOCK           = 1

} AML_LOCK_RULE;


/* Field Update Rules */

#define UPDATE_RULE_MASK        0x060
#define UPDATE_RULE_SHIFT       5

typedef enum
{
    UPDATE_PRESERVE             = 0,
    UPDATE_WRITE_AS_ONES        = 1,
    UPDATE_WRITE_AS_ZEROS       = 2

} AML_UPDATE_RULE;


/* bit fields in MethodFlags byte */

#define METHOD_FLAGS_ARG_COUNT  0x07
#define METHOD_FLAGS_SERIALIZED 0x08


/* Array sizes.  Used for range checking also */

#define NUM_REGION_TYPES        7
#define NUM_ACCESS_TYPES        7
#define NUM_UPDATE_RULES        3
#define NUM_MATCH_OPS           7
#define NUM_OPCODES             256
#define NUM_FIELD_NAMES         2


#define USER_REGION_BEGIN       0x80

/*
 * AML tables
 */

#ifdef DEFINE_AML_GLOBALS

/* External declarations of the AML tables */

extern UINT8                    AcpiGbl_Aml             [NUM_OPCODES];
extern UINT16                   AcpiGbl_Pfx             [NUM_OPCODES];


#endif /* DEFINE_AML_GLOBALS */

#endif /* __AMLCODE_H__ */
