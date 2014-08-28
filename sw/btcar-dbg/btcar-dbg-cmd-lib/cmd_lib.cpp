/***C*********************************************************************************************
**
** SRC-FILE     :   cmd_lib.cpp
**                                        
** PROJECT      :   BTCAR
**                                                                
** SRC-VERSION  :   
**              
** DATE         :   
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   
**                  
** COPYRIGHT    :   
**
****C*E******************************************************************************************/

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "cmd_lib.h"

//---------------------------------------------------------------------------

T_CP_CMD* lookup_cp_cmd(WCHAR *pc_cmd_arg, T_CP_CMD *pt_cmd_lib){

    while(pt_cmd_lib->pc_name){
        if (_wcsicmp(pc_cmd_arg, pt_cmd_lib->pc_name) == 0){
            break;
        }
        pt_cmd_lib++;
    } // end of command scan while

    return pt_cmd_lib;
}

int update_cmd_int(WCHAR *pc_cmd_arg, T_CP_CMD_FIELD *pt_fields, int n_update){

    int  n_rc;
    int  n_value;
    WCHAR  *pc_cmd, *pc_value;
    int n_skip_tokenization_fl;
    T_CP_CMD_FIELD *pt_curr_field;

    n_rc = TRUE;

    if (!(*pc_cmd_arg)) return n_rc;

    pc_cmd = wcstok(pc_cmd_arg, L" ");
    
    n_skip_tokenization_fl = 0;

    while(pc_cmd){
        if (pc_cmd[0] != L'-') {
            fwprintf(stderr, L"Command field name must follow '-' symbol\n");
            n_rc = FALSE;
            break;
        }else {
            pc_cmd++; // Move pointer over '-'
        }

        // Scan fields table and find cmd field in fields table
        pt_curr_field = pt_fields;

        n_rc = FALSE;
        while(pt_curr_field->pc_name){
            if (_wcsicmp(pt_curr_field->pc_name, pc_cmd) == 0){
                // Field name found here

                // Get Field value
                pc_value = wcstok(NULL, L" ");
                if (pc_value ==  NULL){
                    fwprintf(stderr, L"Value not specified \n", pc_value);
                    break;
                }

                // Field name was found and valu exist

                // Update field if requried
                switch (pt_curr_field->e_type){
                    case CFT_NUM: 
                        if (swscanf_s( pc_value, L"%i", &n_value) != 1){
                            fwprintf(stderr, L"Cannot recognize value %s \n", pc_value);
                            break;
                        }
                        
                        if (n_update) {
                            pt_curr_field->dw_val = (DWORD)n_value;
                        }
                        break;

                    case CFT_TXT: 
                        WCHAR *pc_dst = pt_curr_field->pc_str;
                        int n_dst_size = pt_curr_field->n_len;

                        while(pc_value){
                            if (n_update){
                                wcsncpy_s(pc_dst, n_dst_size+1, pc_value, _TRUNCATE);
                            }
                            pc_dst += wcslen(pc_value);
                            n_dst_size -= wcslen(pc_value);

                            pc_value = wcstok(NULL, L" ");

                            // check is another token started
                            if (pc_value && pc_value[0] == L'-') 
                            {
                                n_skip_tokenization_fl = true;
                                pc_cmd = pc_value;
                                break;
                            }

                            if (pc_value && n_dst_size > 0){
                                *pc_dst++ = L' ';
                                n_dst_size--;
                            }
                        }
                        break;

                } // End of field type switch

                n_rc = TRUE;
                break;  // break from field scan

            }// End of field match
            pt_curr_field++;
        } // End of field scan parse
 
        // Check field not found
        if (!pt_curr_field->pc_name){
            fwprintf(stderr, L"Field %s not recognized\n", pc_cmd);
        }

        // l_rc == TRUE only if field found and specified value fit to field
        if (!n_rc) break;

        // Read next field from argument string
        if (!n_skip_tokenization_fl)
            pc_cmd = wcstok(NULL, L" ");

        n_skip_tokenization_fl = false;

    }// End of Command parse while

    return n_rc;
}

T_CP_CMD* decomposite_cp_cmd(WCHAR *pc_cmd_arg, T_CP_CMD *pt_cmd, int n_update){

#define MAX_CMD_ARG_LEN 1024

    WCHAR   ca_cmd[MAX_CMD_ARG_LEN];
    WCHAR   *pc_cmd_token, *pc_cmd_line_end;

    int n_rc = FALSE;

    // Make command local copy to use STRTOK
    if (!(*pc_cmd_arg)) return NULL;
    wcscpy_s(ca_cmd, MAX_CMD_ARG_LEN, pc_cmd_arg);

    // Read command
    pc_cmd_token = wcstok(ca_cmd, L" ");
    if (!(*pc_cmd_token)) return NULL;

    pt_cmd = lookup_cp_cmd(pc_cmd_token, pt_cmd);

    // Check is command was found, update fields if so
    if (pt_cmd->pt_fields){

        // Calc last element address
        pc_cmd_line_end = &ca_cmd[wcslen(pc_cmd_arg)];

        // Move pointer over command if not end of line
        pc_cmd_token += wcslen(pc_cmd_token);
        if (pc_cmd_token != pc_cmd_line_end) pc_cmd_token ++;

        // Parse command WITH fields update
        n_rc = update_cmd_int(pc_cmd_token, pt_cmd->pt_fields, n_update);
    }else{
        fwprintf(stderr, L"Command not found %s\n", pc_cmd_token);
    }

    return (n_rc ? pt_cmd : NULL);
}

