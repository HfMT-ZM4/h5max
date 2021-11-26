/* 
   https://support.hdfgroup.org/ftp/HDF5/examples/examples-by-api/hdf5-examples/1_8/C/H5G/h5ex_g_traverse.c
*/

#include "hdf5.h"
#include <stdio.h>

struct opdata
{
    unsigned recurs;            /* recursion level; 0=root */
    struct opdata *prev;        /* link */
    haddr_t addr;               /* group address */
};

int group_check(struct opdata *od, haddr_t target_addr)
{
    if(od->addr == target_addr)
    {
        return 1;
    }
    else if(!od->recurs)
    {
        return 0;
    }
    else
    {
        return group_check(od->prev, target_addr);
    }
    return 0;
}

herr_t op_func(hid_t loc_id,
               const char *name,
               const H5L_info_t *info,
               void *operator_data)
{
    herr_t status, return_val = 0;
    H5O_info_t infobuf;
    struct opdata *od = (struct opdata *)operator_data;
    unsigned spaces = 2 * (od->recurs + 1);

    status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
    printf("%*s", spaces, "");
    switch(infobuf.type)
    {
    case H5O_TYPE_GROUP:
        printf("Group: %s {\n", name);
        if(group_check(od, infobuf.addr))
        {
            printf("%*s Warning: Loop detected!\n", spaces, "");
        }
        else
        {
            /* recursively examine the group */
            struct opdata nextod;
            nextod.recurs = od->recurs + 1;
            nextod.prev = od;
            nextod.addr = infobuf.addr;
            return_val = H5Literate_by_name(loc_id,
                                            name,
                                            H5_INDEX_NAME,
                                            H5_ITER_NATIVE,
                                            NULL,
                                            op_func,
                                            (void *)&nextod,
                                            H5P_DEFAULT);
        }
        printf("%*s}\n", spaces, "");
        break;
    case H5O_TYPE_DATASET:
        printf("Dataset: %s\n", name);
        break;
    case H5O_TYPE_NAMED_DATATYPE:
        printf("Datatype: %s\n", name);
        break;
    default:
        printf("Unknown: %s\n", name);
    }
    return return_val;
}

int main(int ac, char **av)
{
    hid_t file;
    herr_t status;
    H5O_info_t infobuf;
    struct opdata od;

    file = H5Fopen(av[1], H5F_ACC_RDONLY, H5P_DEFAULT);
    status = H5Oget_info(file, &infobuf);
    od.recurs = 0;
    od.prev = NULL;
    od.addr = infobuf.addr;

    printf("/ {\n");
    status = H5Literate(file,
                        H5_INDEX_NAME,
                        H5_ITER_NATIVE,
                        NULL,
                        op_func,
                        (void *)&od);
    printf("}\n");

    status = H5Fclose(file);
    return 0;
}
