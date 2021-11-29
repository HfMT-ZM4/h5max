/**
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_path.h"
#include "hdf5.h"

#define H5FILE_OUTLET_MAIN 0
#define H5FILE_OUTLET_INFO 1

void *h5file_class;

typedef struct _h5file
{
	t_object ob;
    void *outlets[2];
    t_symbol *filename;
    t_symbol *filepath;
    hid_t fileid;
} h5file;

t_symbol *ps_emptystring, *ps_info, *ps_dataset, *ps_group,
    *ps_floatingpoint, *ps_orderle, *ps_orderbe, *ps_hyperslab;

static void h5file_getHyperslab(h5file *x,
                                t_symbol *s,
                                long ac,
                                t_atom *av)
{
    /* 
       gethyperslab Temperature 0 10 0 10 0 10
    */
    hid_t file = x->fileid;
    if(file <= 0)
    {
        object_error((t_object *)x, "no file open");
        return;
    }
    if(ac < 3)
    {
        object_error((t_object *)x,
                     "requires at least three arguments: "
                     "name of the dataspace, dim0 offset, dim0 count, "
                     "... <dimN offset> <dimN count>");
        return;
    }
    if(atom_gettype(av) != A_SYM)
    {
        object_error((t_object *)x,
                     "first argument must be a symbol "
                     "(the name of the dataspace)");
        return;
    }
    t_symbol *name = atom_getsym(av);

    hid_t dataset = H5Dopen2(file, name->s_name, H5P_DEFAULT);
    if(dataset < 0)
    {
        object_error((t_object *)x,
                     "couldn't open dataset %s",
                     name->s_name);
        return;
    }
    hid_t datatype = H5Dget_type(dataset);
    if(datatype < 0)
    {
        object_error((t_object *)x,
                     "couldn't open datatype for dataset %s",
                     name->s_name);
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
                     name->s_name);
        H5Sclose(datatype);
        H5Sclose(dataset);
        return;
    }
    int rank = H5Sget_simple_extent_ndims(dataspace);
    if(rank != ((ac - 1) / 2))
    {
        object_error((t_object *)x,
                     "expected %d args, but got %d",
                     rank + 1, ac);
        H5Sclose(dataspace);
        H5Sclose(datatype);
        H5Sclose(dataset);
        return;
    }
    hsize_t indims[rank];
    int status_n = H5Sget_simple_extent_dims(dataspace, indims, NULL);
    hsize_t offsets[rank];
    hsize_t counts[rank];
    hsize_t n = 1;
    for(int i = 0; i < rank; i++)
    {
        long offset = atom_getlong(av + 1 + (i * 2));
        long count = atom_getlong(av + 1 + (i * 2 + 1));
        n *= count;
        if(offset + count >= indims[i])
        {
            object_error((t_object *)x,
                         "index for dimension %d out of bounds:"
                         "%d + %d >= %d",
                         i, offset, count, indims[i]);
            H5Sclose(dataspace);
            H5Sclose(datatype);
            H5Sclose(dataset);
            return;
        }
        offsets[i] = offset;
        counts[i] = count;
    }
    herr_t status = H5Sselect_hyperslab(dataspace,
                                        H5S_SELECT_SET,
                                        offsets, NULL,
                                        counts, NULL);
    if(status < 0)
    {
        object_error((t_object *)x, "error selecting hyperslab");
        H5Sclose(dataspace);
        H5Sclose(datatype);
        H5Sclose(dataset);
    }
    hid_t memspace = H5Screate_simple(rank, counts, NULL);
    if(memspace < 0)
    {
        object_error((t_object *)x, "error creating memspace");
        H5Sclose(dataspace);
        H5Sclose(datatype);
        H5Sclose(dataset);
    }
    hsize_t offsets_out[rank];
    memset(offsets_out, 0, rank * sizeof(hsize_t));
    status = H5Sselect_hyperslab(memspace,
                                 H5S_SELECT_SET,
                                 offsets_out, NULL,
                                 counts, NULL);

    const size_t outoffset = 0;
    t_atom *out = (t_atom *)malloc(((n + outoffset) * sizeof(t_atom)));
    
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
                status = H5Dread(dataset,
                                 H5T_IEEE_F32LE,
                                 memspace,
                                 dataspace,
                                 H5P_DEFAULT,
                                 (void *)d);
                
                for(int i = 0; i < n; i++)
                {
                    atom_setfloat(out + i + outoffset, d[i]);
                }
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
                status = H5Dread(dataset,
                                 H5T_IEEE_F64LE,
                                 memspace,
                                 dataspace,
                                 H5P_DEFAULT,
                                 (void *)d);
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
	h5file *x = object_alloc(h5file_class);
    if(x == NULL)
    {
        return NULL;
    }
    /* x->name = NULL; */
    /* attr_args_process(x, ac, av); */
    /* if(x->name) */
    /* { */
    /*     x->name_mangled = h5max_mangleName(x->name); */
    /* } */
    x->outlets[H5FILE_OUTLET_INFO] = outlet_new((t_object *)x, NULL);
    x->outlets[H5FILE_OUTLET_MAIN] = outlet_new((t_object *)x, NULL);
    if(ac)
    {
        if(atom_gettype(av) != A_SYM)
        {
            object_error((t_object *)x,
                         "first argument must be a symbol (file name)");
        }
        else
        {
            h5file_open(x, atom_getsym(av));
        }
    }
	return x;
}

void ext_main(void *r)
{
	t_class *c = class_new("h5.file",
                           (method)h5file_new,
                           (method)h5file_free,
                           (short)sizeof(h5file),
                           0L, A_GIMME, 0);

    class_addmethod(c, (method)h5file_info, "info", 0);
    class_addmethod(c, (method)h5file_getHyperslab, "gethyperslab", A_GIMME, 0);
	class_addmethod(c, (method)h5file_open, "open", A_DEFSYM, 0);
    class_addmethod(c, (method)h5file_close, "close", 0);
	class_addmethod(c, (method)h5file_assist,	"assist", A_CANT, 0);

    /* CLASS_ATTR_SYM(c, "name", 0, h5file, name); */
    /* CLASS_ATTR_ACCESSORS(c, "name", */
    /*                      h5file_name_get, */
    /*                      h5file_name_set); */
    
	class_register(CLASS_BOX, c);
	h5file_class = c;
    
	ps_emptystring = gensym("");
    ps_dataset = gensym("dataset");
    ps_group = gensym("group");
    ps_floatingpoint = gensym("floating point");
    ps_orderle = gensym("little endian");
    ps_orderbe = gensym("big endian");
    ps_hyperslab = gensym("hyperslab");
}
