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
} T_DUMMY, *PT_DUMMY;


typedef union named_union_b_e { 
    uint8_t  union_nb1;
    uint16_t union_nb2;
    T_DUMMY  dummy_s;
    T_DUMMY  *p_dummy_s;
    PT_DUMMY  p_pdummy_s;
} T_UNION_NB;

typedef struct sign_struct T_SIGN_STRUCT;

typedef struct {
    struct sign_struct {
        void *ptr_void;
        int  int_a;
        uint8_t  uc_ver_maj;
        uint8_t  uc_ver_min;
        char     ca_sign[SIGN_LEN];
    } t_sign1;
    T_SIGN_STRUCT t_sign2;
    void (*pf_led_func)(uint8_t);
    uint8_t  reserved[128-sizeof(struct sign_struct) - sizeof(void*)];
}HW_INFO;


typedef enum _enum_a_e {
    enum_str_A = 1,
    enum_str_B = 2,
    enum_str_C = 5,
    enum_str_D = 10
} T_ENUM_A;
    
typedef struct {
    HW_INFO *pointer_A;
    HW_INFO  struct_A;
    struct test_struct_B {

        union {      				// Unnamed union
            uint8_t  union_A1;
            uint16_t union_A2;
        };

        union {        				// Named union
            uint8_t  union_B1;
            uint16_t union_B2;
        } named_union_B;
	
	T_ENUM_A enum_B;

        T_DUMMY  str_str_A;         // Structure in structure through typedef
        uint8_t  field_A;
        uint8_t  field_B;
        char     array_A[5];
        char     array_AA[2][3];
    } named_struct_B;                     // in place defined structure in structure

    struct {
        uint8_t  field_A;
        uint8_t  field_B;
    } unnamed_struct_C;                     // in place defined structure in structure

    
    void (*pf_func_A)(uint8_t);
    void (*pf_func_B)(HW_INFO *hw_info_p);
    T_DUMMY struct_array_BB[3][2];              // Array of structures

    T_ENUM_A enum_A;

} DBGLOG_TEST;   

typedef struct {
    short n_array[2][3][2];
} T_N_ARRAY_SBASE;

typedef char ARRAY_TYPE_BASE_1  [2];
typedef short ARRAY_TYPE_BASE_2 [2];

ARRAY_TYPE_BASE_1 arr_type_1 = {1, 2};
ARRAY_TYPE_BASE_2 arr_type_2 = {1, 2};

typedef struct {
    void (*pf_func_A)(uint8_t);
    void (*pf_func_B)(HW_INFO *hw_info_p);
    HW_INFO* (*pf_func_C)(void);
} T_FUNC_STRUCT;


T_FUNC_STRUCT struct_func;

typedef struct {
    uint8_t n_array[2][3];
    T_ENUM_A enum_A;
    char     ch_A; 
} T_N_ARRAY_STYPE;

// TODO: 
//     1) Link to last DIE entry
//     2) Linked entry is LAST

DBGLOG_TEST	dbglog_test;

T_N_ARRAY_SBASE   n_array_sbase = {
    {
     {{1 ,2}, {3,  4}, {5,   6}},
     {{7 ,8}, {9, 10}, {11, 12}}
    }
};

T_N_ARRAY_STYPE    n_array_stype = {
   {
     {1, 2, 3},
     {3, 4, 5}
   },
   enum_str_B,
   'A'
};

T_UNION_NB named_union_b;

HW_INFO gt_hw_info;


void init_dbglog_test_data()
{
    dbglog_test.pointer_A = (HW_INFO *)(0x11111111);

    memset(&dbglog_test.struct_A, 0x55, sizeof(dbglog_test.struct_A));

    dbglog_test.unnamed_struct_C.field_A = 0xCC;
    dbglog_test.unnamed_struct_C.field_B = 0xDD;

    dbglog_test.named_struct_B.union_A1 = 0x22;
    dbglog_test.named_struct_B.union_A2 = 0x3344;
    dbglog_test.named_struct_B.named_union_B.union_B1 = 0x55;
    dbglog_test.named_struct_B.named_union_B.union_B1 = 0x66;
    dbglog_test.named_struct_B.str_str_A.int8_A = 0x77;
    dbglog_test.named_struct_B.str_str_A.int16_A = 0x8899;
    dbglog_test.named_struct_B.field_A = 0xAA;
    dbglog_test.named_struct_B.field_B = 0xBB;

    dbglog_test.named_struct_B.array_A[0] = 0x00;
    dbglog_test.named_struct_B.array_A[1] = 0x11;
    dbglog_test.named_struct_B.array_A[2] = 0x22;
    dbglog_test.named_struct_B.array_A[3] = 0x33;
    dbglog_test.named_struct_B.array_A[4] = 0x44;

    dbglog_test.named_struct_B.array_AA[0][0] = 0x00;
    dbglog_test.named_struct_B.array_AA[0][1] = 0x01;
    dbglog_test.named_struct_B.array_AA[0][2] = 0x02;
                                        
    dbglog_test.named_struct_B.array_AA[1][0] = 0x10;
    dbglog_test.named_struct_B.array_AA[1][1] = 0x11;
    dbglog_test.named_struct_B.array_AA[1][2] = 0x12;

    dbglog_test.pf_func_A = (void*)(0x12345678);
    dbglog_test.pf_func_B = (void*)(0x87654321);
    
    memset(&dbglog_test.struct_array_BB[0][0], 0x00, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[0][1], 0x01, sizeof(T_DUMMY));
                                        
    memset(&dbglog_test.struct_array_BB[1][0], 0x10, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[1][1], 0x11, sizeof(T_DUMMY));
                                        
    memset(&dbglog_test.struct_array_BB[2][0], 0x20, sizeof(T_DUMMY));
    memset(&dbglog_test.struct_array_BB[2][1], 0x21, sizeof(T_DUMMY));

    dbglog_test.enum_A = enum_str_C;

    gt_hw_info.t_sign1.uc_ver_maj = 0x12;
    gt_hw_info.t_sign1.uc_ver_min = 0x34;
    strncpy(gt_hw_info.t_sign1.ca_sign, "Test string", SIGN_LEN);
    gt_hw_info.pf_led_func = (void*)init_dbglog_test_data;

}


int main (void)
{
    T_SIGN_STRUCT  *sign_ptr;

    uint8_t uc_i = 0x77;
    T_ENUM_A  test_enum = enum_str_C;

    init_dbglog_test_data();

    out_file = fopen("dbg_log_out.bin", "wb");
    if (out_file == NULL)
    {
        printf("Can't create output files.");
        return 0;
    }

    DBG_LOG_D("Just note", NULL);

    DBG_LOG_D("Just note 1st line"
              "Just note 2nd line", 
              NULL
    );

    sign_ptr = &gt_hw_info.t_sign1;
    DBG_LOG_D("signature %(T_SIGN_STRUCT).",
              VAR2ADDR_SIZE(*sign_ptr));

    DBG_LOG_D("HW info  %(HW_INFO) just digit %(uint8_t)",
              VAR2ADDR_SIZE(gt_hw_info),
              VAR2ADDR_SIZE(uc_i));

    DBG_LOG_D("Structure of Functions %(T_FUNC_STRUCT)",
              VAR2ADDR_SIZE(struct_func));

    DBG_LOG_D("Named union %(T_UNION_NB)",
              VAR2ADDR_SIZE(named_union_b));

    DBG_LOG_D("Base type test  %(unsigned char)", VAR2ADDR_SIZE(uc_i));

    DBG_LOG_D("testing typedef enums  %(T_ENUM_A)", VAR2ADDR_SIZE(test_enum));

    DBG_LOG_D("testing Arrays  %(ARRAY_TYPE_BASE_1)", VAR2ADDR_SIZE(arr_type_1));

    DBG_LOG_D("testing Arrays  %(ARRAY_TYPE_BASE_2)", VAR2ADDR_SIZE(arr_type_2));

    DBG_LOG_D("testing_NxArrays typedef %(T_N_ARRAY_STYPE)", VAR2ADDR_SIZE(n_array_stype));

    DBG_LOG_D("testing_NxArrays typedef %(T_N_ARRAY_SBASE)", VAR2ADDR_SIZE(n_array_sbase));


    DBG_LOG_D("Mega struct %(DBGLOG_TEST)",
              VAR2ADDR_SIZE(dbglog_test));


    fclose(out_file);
    printf("Ready. Output written to dbg_log_out.bin\n");
    
    return 0;
}

