/* 
   https://raw.githubusercontent.com/HDFGroup/hdf5/develop/examples/h5_rdwt.c
*/

#include "hdf5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int ac, char **av)
{
    hid_t file = H5Fopen(av[1], H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t dataset = H5Dopen2(file, av[2], H5P_DEFAULT);
    hid_t datatype = H5Dget_type(dataset);
    H5T_class_t class = H5Tget_class(datatype);
    if(class == H5T_FLOAT)
    {
        printf("yay, they're floats!\n");
    }
    else
    {
        printf("aww, i don't know what type they are :(\n");
    }
    H5T_order_t order = H5Tget_order(datatype);
    if(order == H5T_ORDER_LE)
    {
        printf("little endian\n");
    }
    else
    {
        printf("big endian\n");
    }
    size_t size = H5Tget_size(datatype);
    printf("Data size is %zu\n", size);

    hid_t dataspace = H5Dget_space(dataset);
    int rank = H5Sget_simple_extent_ndims(dataspace);
    printf("rank = %d\n", rank);
    hsize_t dims[rank];
    int status_n = H5Sget_simple_extent_dims(dataspace, dims, NULL);
    for(int i = 0; i < rank; i++)
    {
        printf("dim %d: %llu\n", i, dims[i]);
    }

    hsize_t offset[] = {0, 0, 0};
    hsize_t count[] = {900, 900, 900};
    herr_t status = H5Sselect_hyperslab(dataspace,
                                        H5S_SELECT_SET,
                                        offset,
                                        NULL,
                                        count,
                                        NULL);

    hsize_t dims_mem[] = {900, 900, 900};
    hid_t memspace = H5Screate_simple(rank, dims_mem, NULL);

    hsize_t offset_out[] = {0, 0, 0};
    hsize_t count_out[] = {900, 900, 900};
    status = H5Sselect_hyperslab(memspace,
                                 H5S_SELECT_SET,
                                 offset_out,
                                 NULL,
                                 count_out,
                                 NULL);
    //float data_out[10][10][10];
    float *data_out = (float *)calloc(900 * 900 * 900, sizeof(float));
    //memset(data_out, 0, 10 * 10 * 10 * sizeof(float));
    status = H5Dread(dataset,
                     H5T_IEEE_F32LE,
                     memspace,
                     dataspace,
                     H5P_DEFAULT,
                     data_out);

    for(int i = 0; i < 10; i++)
    {
        for(int j = 0; j < 10; j++)
        {
            for(int k = 0; k < 10; k++)
            {
                //printf("[%d,%d,%d] = %f\n", i, j, k, data_out[i][j][k]);
                printf("[%d,%d,%d] = %f\n", i, j, k,
                       data_out[(i * 900) + (j * 900) + k]);
            }
        }
    }
    
    status = H5Dclose(dataset);
    status = H5Fclose(file);

    return 0;
}
