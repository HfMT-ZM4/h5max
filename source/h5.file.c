/**
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_path.h"
#include "jit.common.h"
#include "max.jit.mop.h"
#include "hdf5.h"

#include "h5.file.jit.h"

#define H5FILE_OUTLET_MAIN 0
#define H5FILE_OUTLET_INFO 1

#define H5FILE_MAX_RANK 16

void *h5file_class;

typedef struct _h5file
{
	t_object ob;
    void *obex;
    void *outlets[2];
    t_symbol *filename;
    t_symbol *filepath;
    hid_t fileid;

    void *data;
    int datatype;
    size_t datalen;
    size_t datumsize;
    int rank;
    hsize_t dims[JIT_MATRIX_MAX_DIMCOUNT];

    t_symbol *jit_matrix_name;

} h5file;

t_symbol *ps_emptystring, *ps_info, *ps_dataset, *ps_group,
    *ps_floatingpoint, *ps_orderle, *ps_orderbe, *ps_hyperslab,
    *ps_coords, *ps_vals;

t_jit_err h5file_jit_init(void);

static void postOutOfMemError(h5file *x, size_t nbytes)
{
    object_error((t_object *)x, "couldn't allocate %d bytes", nbytes);
}

static void h5file_outputJitterMatrix(h5file *x,
                                      void *data,
                                      int datatype,
                                      size_t datalen,
                                      size_t datumsize,
                                      int rank,
                                      hsize_t *dims)
{
    if(x->data)
    {
        if(x->datalen * x->datumsize <
           datalen * datumsize)
        {
            x->data = realloc(data, datalen * datumsize);
        }
    }
    else
    {
        x->data = malloc(datalen * datumsize);
    }
    if(!x->data)
    {
        postOutOfMemError(x, datalen * datumsize);
        return;
    }
    memcpy(x->data, data, datalen * datumsize);
    x->datatype = datatype;
    x->datalen = datalen;
    x->rank = rank;
    memcpy(x->dims, dims, rank * sizeof(hsize_t));
    t_atom a;
    long outputmode = max_jit_mop_getoutputmode(x);
    h5file_jit *jit_obj = (h5file_jit *)max_jit_obex_jitob_get(x);
    t_atom dim[rank];
    for(int i = 0; i < rank; i++)
    {
        atom_setlong(dim + i, dims[i]);
    }
    jit_object_method_typed(x, gensym("dim"), rank, dim, NULL);
    switch(datatype)
    {
    case H5T_FLOAT:
        atom_setsym(dim, _jit_sym_float32);
        jit_object_method_typed(x, gensym("type"), 1, dim, NULL);
        break;
    }
    atom_setlong(dim, 1);
    jit_object_method_typed(x, gensym("planecount"), 1, dim, NULL);
    jit_obj->data = x->data;
    jit_obj->datatype = x->datatype;
    jit_obj->datalen = x->datalen;
    void *mop = max_jit_obex_adornment_get(x, _jit_sym_jit_mop);
    t_jit_err err;
    if(outputmode && mop)
    {
        /* not sure i understand this output mode */
        if(outputmode == 1)
        {
            if((err = (t_jit_err)jit_object_method(
                    jit_obj,
                    _jit_sym_matrix_calc,
                    jit_object_method(mop, _jit_sym_getinputlist),
                    jit_object_method(mop, _jit_sym_getoutputlist))))
            {
                jit_error_code(x, err);
            }
            else
            {
                max_jit_mop_outputmatrix(x);
            }
        }
        else
        {
            max_jit_mop_outputmatrix(x);
        }
    }
}

static void h5file_getHyperslab(h5file *x,
                                t_symbol *s,
                                long ac,
                                t_atom *av)
{
    /* 
       example message we expect:
       gethyperslab Temperature 0 10 0 10 0 10
    */
    const t_symbol * const datasetname = atom_getsym(av);
    const char * const datasetname_str = datasetname->s_name;
    const size_t outoffset = 0;
    t_atom *out = NULL;

    hsize_t n = 1;
    herr_t status = 0;
    
    hid_t file = x->fileid;
    hid_t dataset = -1;
    hid_t datatype = -1;
    H5T_class_t class = -1;
    H5T_order_t order = -1;
    size_t size = 0;
    hid_t dataspace = -1;
    int rank = -1;
    hsize_t indims[H5FILE_MAX_RANK];
    memset(indims, 0, sizeof(hsize_t) * H5FILE_MAX_RANK);
    hsize_t offsets[H5FILE_MAX_RANK];
    memset(offsets, 0, sizeof(hsize_t) * H5FILE_MAX_RANK);
    hsize_t counts[H5FILE_MAX_RANK];
    memset(counts, 0, sizeof(hsize_t) * H5FILE_MAX_RANK);
    hsize_t offsets_out[H5FILE_MAX_RANK];
    memset(offsets_out, 0, sizeof(hsize_t) * H5FILE_MAX_RANK);
    hid_t memspace = -1;
    
    if(file <= 0)
    {
        object_error((t_object *)x, "no file open");
        goto cleanup;
    }
    if(ac < 3)
    {
        object_error((t_object *)x,
                     "requires at least three arguments: "
                     "name of the dataset, dim0 offset, dim0 count, "
                     "... <dimN offset> <dimN count>");
        goto cleanup;
    }
    if(atom_gettype(av) != A_SYM)
    {
        object_error((t_object *)x,
                     "first argument must be a symbol "
                     "(the name of the dataset)");
        goto cleanup;
    }

    dataset = H5Dopen2(file, datasetname_str, H5P_DEFAULT);
    if(dataset < 0)
    {
        object_error((t_object *)x,
                     "couldn't open dataset %s",
                     datasetname_str);
        goto cleanup;
    }
    datatype = H5Dget_type(dataset);
    if(datatype < 0)
    {
        object_error((t_object *)x,
                     "couldn't open datatype for dataset %s",
                     datasetname_str);
        goto cleanup;
    }
    class = H5Tget_class(datatype);
    order = H5Tget_order(datatype);
    size = H5Tget_size(datatype);

    dataspace = H5Dget_space(dataset);
    if(dataspace < 0)
    {
        object_error((t_object *)x,
                     "couldn't open dataspace for dataset %s",
                     datasetname_str);
        goto cleanup;
    }
    rank = H5Sget_simple_extent_ndims(dataspace);
    if(rank > H5FILE_MAX_RANK)
    {
        object_error((t_object *)x,
                     "max rank exceded (%d > %d)",
                     rank, H5FILE_MAX_RANK);
        goto cleanup;
    }
    if(rank != ((ac - 1) / 2))
    {
        object_error((t_object *)x,
                     "expected %d args, but got %d",
                     rank + 1, ac);
        goto cleanup;
    }
    status = H5Sget_simple_extent_dims(dataspace, indims, NULL);
    for(int i = 0; i < rank; i++)
    {
        long offset = atom_getlong(av + 1 + (i * 2));
        long count = atom_getlong(av + 1 + (i * 2 + 1));
        n *= count;
        if(offset + count - 1 >= indims[i])
        {
            object_error((t_object *)x,
                         "index for dimension %d out of bounds:"
                         "%d + %d >= %d",
                         i, offset, count, indims[i]);
            goto cleanup;
        }
        offsets[i] = offset;
        counts[i] = count;
    }
    status = H5Sselect_hyperslab(dataspace,
                                 H5S_SELECT_SET,
                                 offsets, NULL,
                                 counts, NULL);
    if(status < 0)
    {
        object_error((t_object *)x, "error selecting hyperslab");
        goto cleanup;
    }
    memspace = H5Screate_simple(rank, counts, NULL);
    if(memspace < 0)
    {
        object_error((t_object *)x, "error creating memspace");
        goto cleanup;
    }
    status = H5Sselect_hyperslab(memspace,
                                 H5S_SELECT_SET,
                                 offsets_out, NULL,
                                 counts, NULL);

    if(status < 0)
    {
        object_error((t_object *)x, "error selecting hyperslab");
        goto cleanup;
    }
    

    
    out = (t_atom *)malloc(((n + outoffset) * sizeof(t_atom)));
    if(!out)
    {
        postOutOfMemError(x, (n + outoffset) * sizeof(t_atom));
        goto cleanup;
    }
    
    switch(class)
    {
    case H5T_FLOAT:
    {
        switch(size)
        {
        case 4:
        {
            switch(order)
            {
            case H5T_ORDER_LE:
            {
                float *d = (float *)calloc(n, sizeof(float));
                if(!d)
                {
                    postOutOfMemError(x, n * sizeof(float));
                    goto cleanup;
                }
                status = H5Dread(dataset,
                                 H5T_IEEE_F32LE,
                                 memspace,
                                 dataspace,
                                 H5P_DEFAULT,
                                 (void *)d);
                h5file_outputJitterMatrix(x, d, H5T_FLOAT, n, 4,
                                          rank, counts);
                for(int i = 0; i < n; i++)
                {
                    atom_setfloat(out + i + outoffset, d[i]);
                }
                free(d);
            }
            break;
            case H5T_ORDER_BE:
            {

            }
            break;
            default:
            {

            }
            break;
            }
        }
        break;
        case 8:
        {
            switch(order)
            {
            case H5T_ORDER_LE:
            {
                double *d = (double *)calloc(n, sizeof(double));
                if(!d)
                {
                    postOutOfMemError(x, n * sizeof(double));
                    goto cleanup;
                }
                status = H5Dread(dataset,
                                 H5T_IEEE_F64LE,
                                 memspace,
                                 dataspace,
                                 H5P_DEFAULT,
                                 (void *)d);
                for(int i = 0; i < n; i++)
                {
                    atom_setfloat(out + i + outoffset, d[i]);
                }
                free(d);
            }
            break;
            case H5T_ORDER_BE:
            {

            }
            break;
            default:
            {

            }
            break;
            }
        }
        break;
        }
        
    }
    break;
    }
    outlet_anything(x->outlets[H5FILE_OUTLET_MAIN], ps_hyperslab,
                    n + outoffset, out);

cleanup:
    if(out)
    {
    	free(out);
    }
    H5Sclose(memspace);
    H5Sclose(dataspace);
    H5Sclose(datatype);
    H5Sclose(dataset);
}

static void h5file_getDatasetInfo(h5file *x, const char *name)
{
    hid_t file = x->fileid;
    hid_t dataset = H5Dopen2(file, name, H5P_DEFAULT);
    if(dataset < 0)
    {
        object_error((t_object *)x,
                     "couldn't open dataset %s",
                     name);
        return;
    }
    
    hid_t datatype = H5Dget_type(dataset);
    if(datatype < 0)
    {
        object_error((t_object *)x,
                     "couldn't open datatype for dataset %s",
                     name);
        H5Sclose(dataset);
        return;
    }
    
    H5T_class_t class = H5Tget_class(datatype);
    H5T_order_t order = H5Tget_order(datatype);
    size_t size = H5Tget_size(datatype);

    hid_t dataspace = H5Dget_space(dataset);
    if(dataspace < 0)
    {
        object_error((t_object *)x,
                     "couldn't open dataspace for dataset %s",
                     name);
        H5Sclose(datatype);
        H5Sclose(dataset);
        return;
    }
    int rank = H5Sget_simple_extent_ndims(dataspace);
    hsize_t dims[rank];
    int status_n = H5Sget_simple_extent_dims(dataspace, dims, NULL);

    int n = 5 + rank;
    t_atom out[n];
    atom_setsym(out, gensym(name));
    switch(class)
    {
    case H5T_FLOAT:
        atom_setsym(out + 1, ps_floatingpoint);
        break;
    default:
        break;
    }
    switch(order)
    {
    case H5T_ORDER_LE:
        atom_setsym(out + 2, ps_orderle);
        break;
    case H5T_ORDER_BE:
        atom_setsym(out + 2, ps_orderbe);
        break;
    default:
        break;
    }
    atom_setlong(out + 3, size);
    atom_setlong(out + 4, rank);
    for(int i = 0; i < rank; i++)
    {
        atom_setlong(out + 5 + i, dims[i]);
    }
    
    outlet_anything(x->outlets[H5FILE_OUTLET_MAIN],
                    ps_dataset,
                    n,
                    out);
    H5Sclose(dataspace);
    H5Sclose(datatype);
    H5Sclose(dataset);
}

static herr_t h5file_opfunc(hid_t loc_id,
                            const char *name,
                            const H5L_info_t *info,
                            void *operator_data)
{
    h5file *x = (h5file *)operator_data;
    herr_t status;
    H5O_info_t infobuf;
    status = H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT);
    t_atom a[2];
    switch(infobuf.type)
    {
    case H5O_TYPE_GROUP:
        /* post("	Group: %s\n", name); */
        break;
    case H5O_TYPE_DATASET:
        /* post("	Dataset: %s\n", name); */
        /* atom_setsym(a, ps_dataset); */
        /* atom_setsym(a + 1, gensym(name)); */
        /* outlet_anything(x->outlets[H5FILE_OUTLET_MAIN], */
        /*                 ps_info, 2, a); */
        h5file_getDatasetInfo(x, name);
        break;
    case H5O_TYPE_NAMED_DATATYPE:
        /* post("	Datatype: %s\n", name); */
        break;
    default:
        /* post("	Unknown: %s\n", name); */
        break;
    }
    return 0;
}

static void h5file_info(h5file *x)
{
    hid_t f = (hid_t)x->fileid;
    if(!f)
    {
        object_error((t_object *)x,
                     "No file open");
        return;
    }

    {
        herr_t status = H5Literate(f,
                                   H5_INDEX_NAME,
                                   H5_ITER_NATIVE,
                                   NULL,
                                   h5file_opfunc,
                                   (void *)x);
    }
}

static void h5file_reset(h5file *x)
{
    x->filename = NULL;
    x->filepath = NULL;
    x->fileid = 0;
}

static void h5file_doclose(h5file *x)
{
    H5Fclose(x->fileid);
    h5file_reset(x);
}

static void h5file_close(h5file *x)
{
    if(x->fileid)
    {
        object_error((t_object *)x,
                     "No file open");
        return;
    }
	defer(x, (method)h5file_doclose, NULL, 0, 0L);
}

static void h5file_doopen(h5file *x, t_symbol *s, long ac, t_atom *av)
{
	short path;
	char ps[MAX_PATH_CHARS];
	t_fourcc type;

	h5file_doclose(x);
	if(s == ps_emptystring)
    {
		if(open_dialog(ps, &path, &type, 0L, 0))
        {
			return;
        }
	}
    else
    {
		strcpy(ps, s->s_name);
		if(locatefile_extended(ps, &path, &type, &type, 0))
        {
			object_error((t_object *)x, "%s: can't find file", ps);
			return;
		}
	}
	/* err = path_opensysfile(ps, vol, &x->f_fh, READ_PERM); */
	/* if(err) */
    /* { */
	/* 	object_error((t_object *)x, "%s: error %d opening file",ps,err); */
	/* 	return; */
	/* } */
    char abs_sys_path[MAX_PATH_CHARS];
    memset(abs_sys_path, 0, MAX_PATH_CHARS);
    path_toabsolutesystempath(path, ps, abs_sys_path);
    x->filename = gensym(ps);
    x->filepath = gensym(abs_sys_path);
    {
        hid_t fid = H5Fopen(abs_sys_path,
                            H5F_ACC_RDONLY,
                            H5P_DEFAULT);
        if(fid < 0)
        {
            object_error((t_object *)x, "error opening file %s (%lld)",
                         x->filename,
                         fid);
            h5file_reset(x);
            return;
        }
        x->fileid = fid;
    }

	outlet_bang(x->outlets[H5FILE_OUTLET_INFO]);
}

static void h5file_open(h5file *x, t_symbol *s)
{
	defer(x, (method)h5file_doopen, s, 0, 0L);
}

static void h5file_free(h5file *x)
{
    h5file_doclose(x);
    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_obex_free(x);
}

static void h5file_assist(h5file *x, void *b, long m, long a, char *s)
{
	if(m==1)
    {
        // inlets
		switch(a){
		case 0:
            sprintf(s, "Inlet");
            break;
		}
	}
	else if(m==2)
    {
		switch(a)
        {
		case 0:
            sprintf(s, "Outlet");
            break;
		}
	}
}

/* t_max_err h5file_name_set(h5file *x, */
/*                                   t_object *attr, */
/*                                   long argc, */
/*                                   t_atom *argv) */
/* { */
/*     if(argc == 0) */
/*     { */
/*         object_error((t_object *)x, "You must provide a buffer name"); */
/*         return 0; */
/*     } */
/*     if(atom_gettype(argv) != A_SYM) */
/*     { */
/*         object_error((t_object *)x, "Buffer name must be a symbol"); */
/*         return 0; */
/*     } */
/*     x->name = atom_getsym(argv); */
/*     x->name_mangled = h5max_mangleName(x->name); */
/*     return 0; */
/* } */

/* t_max_err h5file_name_get(h5file *x, */
/*                                   t_object *attr, */
/*                                   long *argc, */
/*                                   t_atom **argv) */
/* { */
/*     char alloc; */
/*     atom_alloc(argc, argv, &alloc); */
/*     atom_setsym(*argv, x->name); */
/*     return 0; */
/* } */

static void *h5file_new(t_symbol *sym, long ac, t_atom *av)
{
	/* h5file *x = object_alloc(h5file_class); */
    /* h5file *x = (h5file *)max_jit_obex_new(h5file_class, gensym("h5file_jit")); */
    h5file *x = (h5file *)max_jit_object_alloc(h5file_class, gensym("h5file_jit"));
    if(x == NULL)
    {
        return NULL;
    }
    void *o = jit_object_new(gensym("h5file_jit"));
    if(o)
    {
        max_jit_mop_setup_simple(x, o, ac, av);
        max_jit_attr_args(x, ac, av);
    }
    else
    {
        jit_object_error((t_object *)x,
                         "h5file_jit: could not allocate object");
        freeobject((t_object *)x);
        x = NULL;
        return NULL;
    }
    /* x->name = NULL; */
    /* attr_args_process(x, ac, av); */
    /* if(x->name) */
    /* { */
    /*     x->name_mangled = h5max_mangleName(x->name); */
    /* } */
    /* x->outlets[H5FILE_OUTLET_INFO] = outlet_new((t_object *)x, NULL); */
    max_jit_obex_dumpout_set(x, x->outlets[H5FILE_OUTLET_INFO]);
    x->outlets[H5FILE_OUTLET_MAIN] = outlet_new((t_object *)x, NULL);
    /* x->jit_matrix_name = _jit_sym_nothing; */
    /* long attrstart = max_jit_attr_args_offset(ac, av); */
    /* max_jit_attr_args(x, ac, av); */
    /* if(ac) */
    /* { */
    /*     if(atom_gettype(av) != A_SYM) */
    /*     { */
    /*         object_error((t_object *)x, */
    /*                      "first argument must be a symbol (file name)"); */
    /*     } */
    /*     else */
    /*     { */
    /*         h5file_open(x, atom_getsym(av)); */
    /*     } */
    /* } */
	return x;
}

void ext_main(void *r)
{
    h5file_jit_init();
	t_class *max_class = class_new("h5.file",
                                   (method)h5file_new,
                                   (method)h5file_free,
                                   (short)sizeof(h5file),
                                   0L, A_GIMME, 0);

    /* void *p = max_jit_classex_setup(calcoffset(h5file, obex)); */
    max_jit_class_obex_setup(max_class, calcoffset(h5file, obex));
    void *jit_class = jit_class_findbyname(gensym("h5file_jit"));
    /* max_jit_classex_mop_wrap(p, jit_class, */
    /*                          MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX */
    /*                          | MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX); */
    max_jit_class_mop_wrap(max_class, jit_class,
                           MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX
                           | MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX);
    /* max_jit_classex_standard_wrap(p, jit_class, 0); */
    max_jit_class_wrap_standard(max_class, jit_class, 0);
    /* don't think i need this, since i'll call it from 
       the gethyperslab routine */
    /* max_addmethod_usurp_low((method)h5file_outputJitterMatrix, */
    /*                         "outputmatrix"); */

    /* void *attr = NULL; */

    /* attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_DEFER_LOW; */
    /* long attrflags = 0; */
    /* attr = jit_object_new(_jit_sym_jit_attr_offset, "jit_matrix_name", _jit_sym_symbol, attrflags, */
	/* 					  (method)0L, (method)0L, calcoffset(h5file, jit_matrix_name)); */
    /* object_addattr_parse(attr, "label", _jit_sym_symbol, 0, "\"Jitter Matrix Name\""); */
	/* max_jit_classex_addattr(p, attr); */

    class_addmethod(max_class, (method)h5file_info, "info", 0);
    class_addmethod(max_class, (method)h5file_getHyperslab, "gethyperslab", A_GIMME, 0);
	class_addmethod(max_class, (method)h5file_open, "open", A_DEFSYM, 0);
    class_addmethod(max_class, (method)h5file_close, "close", 0);
	class_addmethod(max_class, (method)h5file_assist,	"assist", A_CANT, 0);

    /* CLASS_ATTR_SYM(c, "name", 0, h5file, name); */
    /* CLASS_ATTR_ACCESSORS(c, "name", */
    /*                      h5file_name_get, */
    /*                      h5file_name_set); */

    /* max_jit_classex_standard_wrap(p, NULL, 0); */
	class_register(CLASS_BOX, max_class);
	h5file_class = max_class;
    
	ps_emptystring = gensym("");
    ps_dataset = gensym("dataset");
    ps_group = gensym("group");
    ps_floatingpoint = gensym("floating point");
    ps_orderle = gensym("little endian");
    ps_orderbe = gensym("big endian");
    ps_hyperslab = gensym("hyperslab");
    ps_coords = gensym("coords");
    ps_vals = gensym("vals");
}
