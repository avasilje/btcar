/*******************************************************************************
 *  
 *  PROJECT
 *    DBG_LOG reference target
 *
 *  FILE
 *    main.c
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/
#include "local_fids.h"
#define LOCAL_FILE_ID FID_MAIN

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "dbg_log.h"

#define SIGN_LEN 32

FILE *out_file;

typedef struct dummy_struct{
    uint16_t int16_A;
    uint8_t int8_A;
} T_DUMMY;

typedef struct {
    struct sign_struct {
        uint8_t  uc_ver_maj;
        uint8_t  uc_ver_min;
        char     ca_sign[SIGN_LEN];
    } t_sign;
    void (*pf_led_func)(uint8_t);
    uint8_t  reserved[128-sizeof(struct sign_struct) - sizeof(void*)];
}HW_INFO;

typedef enum {
    enum_str_A = 1,
    enum_str_B = 2,
    enum_str_C = 5,
    enum_str_D = 10
} enum_A_e;
    
typedef struct {
    HW_INFO *pointer_A;
    HW_INFO  struct_A;
    struct test_struct {
        union {      				// Unnamed union
            uint8_t  union_A1;
            uint16_t union_A2;
        };

        union {        				// Named union
            uint8_t  union_B1;
            uint16_t union_B2;
        } named_union_B;

        T_DUMMY  str_str_A;         // Structure in structure through typedef
        uint8_t  field_A;
        uint8_t  field_B;
        char     array_A[5];
        char     array_AA[2][3];
    } struct_B;                     // in place defined structure in structure
    
    void (*pf_func_A)(uint8_t);
    void (*pf_func_B)(HW_INFO *hw_info_p);
    T_DUMMY struct_array_BB[3][2];              // Array of structures

    enum_A_e enum_A;

}DBGLOG_TEST;   

// TODO: 
//     1) Link to last DIE entry
//     2) Linked entry is LAST

DBGLOG_TEST	dbglog_test;
HW_INFO gt_hw_info;

void init_dbglog_test_data()
{
    dbglog_test.pointer_A = (HW_INFO *)(0x11111111);

    memset(&dbglog_test.struct_A, 0x55, sizeof(dbglog_test.struct_A));

    dbglog_test.struct_B.union_A1 = 0x22;
    dbglog_test.struct_B.union_A2 = 0x3344;
    dbglog_test.struct_B.named_union_B.union_B1 = 0x55;
    dbglog_test.struct_B.named_union_B.union_B1 = 0x66;
    dbglog_test.struct_B.str_str_A.int8_A = 0x77;
    dbglog_test.struct_B.str_str_A.int16_A = 0x8899;
    dbglog_test.struct_B.field_A = 0xAA;
    dbglog_test.struct_B.field_B = 0xBB;

    dbglog_test.struct_B.array_A[0] = 0x00;
    dbglog_test.struct_B.array_A[1] = 0x11;
    dbglog_test.struct_B.array_A[2] = 0x22;
    dbglog_test.struct_B.array_A[3] = 0x33;
    dbglog_test.struct_B.array_A[4] = 0x44;

    dbglog_test.struct_B.array_AA[0][0] = 0x00;
    dbglog_test.struct_B.array_AA[0][1] = 0x01;
    dbglog_test.struct_B.array_AA[0][2] = 0x02;
                                        
    dbglog_test.struct_B.array_AA[1][0] = 0x10;
    dbglog_test.struct_B.array_AA[1][1] = 0x11;
    dbglog_test.struct_B.array_AA[1][2] = 0x12;

    dbglog_test.pf_func_A = (void*)(0x12345678);
    dbglog_test.pf_func_B = (void*)(0x87654321);
    
    memset(&dbglog_test.struct_array_BB[0][0], 0x00, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[0][1], 0x01, sizeof(T_DUMMY));
                                        
    memset(&dbglog_test.struct_array_BB[1][0], 0x10, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[1][1], 0x11, sizeof(T_DUMMY));
                                        
    memset(&dbglog_test.struct_array_BB[2][0], 0x20, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[2][1], 0x21, sizeof(T_DUMMY));

    dbglog_test.enum_A = enum_str_C;

    gt_hw_info.t_sign.uc_ver_maj = 0x12;
    gt_hw_info.t_sign.uc_ver_min = 0x34;
    strncpy(gt_hw_info.t_sign.ca_sign, "Test string", SIGN_LEN);
    gt_hw_info.pf_led_func = (void*)init_dbglog_test_data;

}


void main (void)
{

    uint8_t uc_i = 0x77;

    init_dbglog_test_data();

    out_file = fopen("dbg_log_out.bin", "wb");
    if (out_file == NULL)
    {
        printf("Can't create output files.");
        return;
    }

    DBG_LOG_D("Just note", NULL);

    DBG_LOG_D("Just note 1st line"
              "Just note 2nd line", 
              NULL
    );

    DBG_LOG_D("signature %(sign_struct)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign));

    DBG_LOG_D("signature %(sign_struct) just digit %(uint8_t)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign),
              VAR2ADDR_SIZE(uc_i));

    fclose(out_file);
    printf("Ready. Output written to dbg_log_out.bin\n");
    
    return;
}

