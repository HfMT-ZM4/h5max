/* for data type #defines */
#include "hdf5.h"

#include "jit.common.h"

#include "h5.file.jit.h"

/* typedef struct h5file_jit */
/* { */
/*     t_object ob; */
/*     void *data; */
/*     int datatype; */
/*     size_t datalen; */
/* } h5file_jit; */

void *h5file_jit_class;

void h5file_jit_stuff_float32(h5file_jit *x,
                              long n, t_jit_op_info *out)
{
    float *op = ((float *)out->p);
    float *p = (float *)(x->p);
    long os = out->stride;
    if(os == 1)
    {
        ++n; --op;
        while(--n)
        {
            *++op = *p++;
        }
    }
    else
    {
        while(n--)
        {
            *op = *p++;
            op += os;
        }
    }
    x->p = p;
}

void h5file_jit_calculate_ndim(h5file_jit *x,
                               long dimcount, long *dim,
                               long planecount,
                               t_jit_matrix_info *out_minfo,
                               char *bop)
{
    long n;
    char *op;
    t_jit_op_info out_opinfo;
    if(dimcount < 1)
    {
        return;
    }
    switch(dimcount)
    {
    case 1:
        dim[1] = 1;
    case 2:
        n = dim[0];
        out_opinfo.stride = 1;
        n *= planecount;
        planecount = 1;
        if(out_minfo->type == _jit_sym_float32)
        {
            for(int i = 0; i < dim[1]; i++)
            {
                out_opinfo.p = bop + i * out_minfo->dimstride[1];
                h5file_jit_stuff_float32(x, n, &out_opinfo);
            }
        }
        break;
    default:
        for(int i = 0; i < dim[dimcount - 1]; i++)
        {
            op = bop + i * out_minfo->dimstride[dimcount - 1];
            h5file_jit_calculate_ndim(x, dimcount - 1, dim,
                                      planecount, out_minfo, op);
        }
    }
}

t_jit_err h5file_jit_matrix_calc(h5file_jit *x,
                                 void *inputs, void *outputs)
{
    t_jit_err err = JIT_ERR_NONE;

    void *out_matrix = jit_object_method(outputs, _jit_sym_getindex, 0);
    char *out_bp;
    t_jit_matrix_info out_minfo;
    long out_savelock;
    long dimcount, planecount, dim[JIT_MATRIX_MAX_DIMCOUNT];
    long i;
    if(x && out_matrix)
    {
        out_savelock = (long)jit_object_method(out_matrix,
                                                    _jit_sym_lock, 1);
        
        jit_object_method(out_matrix, _jit_sym_getinfo, &out_minfo);
        
        
        jit_object_method(out_matrix, _jit_sym_getdata, &out_bp);
        if(!out_bp)
        {
            err = JIT_ERR_INVALID_OUTPUT;
            goto cleanup;
        }
        dimcount = out_minfo.dimcount;
        planecount = out_minfo.planecount;
        
        for(i = 0; i < dimcount; i++)
        {
            dim[i] = out_minfo.dim[i];
        }
        x->p = x->data;
        jit_parallel_ndim_simplecalc1((method)h5file_jit_calculate_ndim,
                                      x, dimcount, dim, planecount,
                                      &out_minfo, out_bp, 0);
    }
    else
    {
        return JIT_ERR_INVALID_PTR;
    }
cleanup:
    jit_object_method(out_matrix, _jit_sym_lock, out_savelock);
    return err;
}

void h5file_jit_free(h5file_jit *x)
{
    /* max obj deals with memory */
}

h5file_jit *h5file_jit_new(void)
{
    h5file_jit *x = (h5file_jit *)jit_object_alloc(h5file_jit_class);
    return x;
}

t_jit_err h5file_jit_init(void)
{
    void *mop;
    h5file_jit_class = jit_class_new("h5file_jit",
                                     (method)h5file_jit_new,
                                     (method)h5file_jit_free,
                                     sizeof(h5file_jit), 0L);
    /* add mop; no inputs, one output */
    mop = jit_object_new(_jit_sym_jit_mop, 0, 1);
    jit_class_addadornment(h5file_jit_class, mop);
    /* method to do the work */
    jit_class_addmethod(h5file_jit_class,
                        (method)h5file_jit_matrix_calc,
                        "matrix_calc", A_CANT, 0L);
    jit_class_register(h5file_jit_class);
    return JIT_ERR_NONE;
}
