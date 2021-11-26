/* 
   https://bitbucket.hdfgroup.org/projects/HDFFV/repos/hdf5-examples/browse/1_10/C/H5G/h5ex_g_iterate.c?at=fddad1d0485a8026dd8eb48dd2e2bdd749fbc3b5&raw
*/

#include "hdf5.h"
#include <stdio.h>

herr_t op_func(hid_t loc_id,
               const char *name,
               const H5L_info_t *info,
               void *operator_data)
{
    herr_t status;
    H5O_info_t infobuf;
    status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
    switch(infobuf.type)
    {
    case H5O_TYPE_GROUP:
        printf("	Group: %s\n", name);
        break;
    case H5O_TYPE_DATASET:
        printf("	Dataset: %s\n", name);
        break;
    case H5O_TYPE_NAMED_DATATYPE:
        printf("	Datatype: %s\n", name);
        break;
    default:
        printf("	Unknown: %s\n", name);
    }
    return 0;
}

int main(int ac, char **av)
{
    hid_t file;
    herr_t status;

    printf("Opening %s\n", av[1]);
    file = H5Fopen(av[1], H5F_ACC_RDONLY, H5P_DEFAULT);

    printf("Objects in root group:\n");
    status = H5Literate(file,
                        H5_INDEX_NAME,
                        H5_ITER_NATIVE,
                        NULL,
                        op_func,
                        NULL);
    
    return 0;
}
