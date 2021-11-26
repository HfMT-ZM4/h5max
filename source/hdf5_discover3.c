/* 
   https://support.hdfgroup.org/ftp/HDF5/examples/examples-by-api/hdf5-examples/1_8/C/H5G/h5ex_g_visit.c
*/

#include "hdf5.h"
#include <stdio.h>

herr_t op_func(hid_t loc_id,
               const char *name,
               const H5O_info_t *info,
               void *operator_data)
{
    printf("/");
    if(name[0] == '.')
    {
        printf("	(Group)\n");
    }
    else
    {
        switch(info->type)
        {
        case H5O_TYPE_GROUP:
            printf("%s	(Group)\n", name);
            break;
        case H5O_TYPE_DATASET:
            printf("%s	(DATASET)\n", name);
            break;
        case H5O_TYPE_NAMED_DATATYPE:
            printf("%s	(Datatype)\n", name);
            break;
        default:
            printf("%s	(Unknown)\n", name);
        }
    }
    return 0;
}

herr_t op_func_L(hid_t loc_id,
               const char *name,
               const H5L_info_t *info,
               void *operator_data)
{
    herr_t status;
    H5O_info_t infobuf;
    status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
    return op_func(loc_id, name, &infobuf, operator_data);
}

int main(int ac, char **av)
{
    hid_t file;
    herr_t status ;

    file = H5Fopen(av[1], H5F_ACC_RDONLY, H5P_DEFAULT);

    printf("Objects in the file:\n");
    status = H5Ovisit(file,
                      H5_INDEX_NAME,
                      H5_ITER_NATIVE,
                      op_func,
                      NULL);

    printf("\nLinks in the file:\n");
    status = H5Lvisit(file,
                      H5_INDEX_NAME,
                      H5_ITER_NATIVE,
                      op_func_L,
                      NULL);

    status = H5Fclose(file);
    return 0;
}
