/**
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_path.h"
#include "hdf5.h"

#define H5FILE_OUTLET_DONEOPENING 0

void *h5file_class;

typedef struct _h5file
{
	t_object ob;
    void *outlets[1];
    t_symbol *filename;
    t_symbol *filepath;
    hid_t fileid;
} h5file;

t_symbol *ps_emptystring;

herr_t h5file_opfunc(hid_t loc_id,
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
        post("	Group: %s\n", name);
        break;
    case H5O_TYPE_DATASET:
        post("	Dataset: %s\n", name);
        break;
    case H5O_TYPE_NAMED_DATATYPE:
        post("	Datatype: %s\n", name);
        break;
    default:
        post("	Unknown: %s\n", name);
    }
    return 0;
}

void h5info_bang(h5file *x)
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
                                   NULL);
    }
}

void h5file_reset(h5file *x)
{
    x->filename = NULL;
    x->filepath = NULL;
    x->fileid = 0;
}

void h5file_doclose(h5file *x)
{
    H5Fclose(x->fileid);
    h5file_reset(x);
}

void h5file_close(h5file *x)
{
    if(x->fileid)
    {
        object_error((t_object *)x,
                     "No file open");
        return;
    }
	defer(x, (method)h5file_doclose, NULL, 0, 0L);
}

void h5file_doopen(h5file *x, t_symbol *s, long ac, t_atom *av)
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

	outlet_bang(x->outlets[H5FILE_OUTLET_DONEOPENING]);
}

void h5file_open(h5file *x, t_symbol *s)
{
	defer(x, (method)h5file_doopen, s, 0, 0L);
}

void h5file_free(h5file *x)
{
    h5file_doclose(x);
}

void h5file_assist(h5file *x, void *b, long m, long a, char *s)
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

void *h5file_new(t_symbol *sym, long ac, t_atom *av)
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
    x->outlets[H5FILE_OUTLET_DONEOPENING] = bangout((t_object *)x);
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

    class_addmethod(c, (method)h5info_bang, "bang", 0);
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
}
