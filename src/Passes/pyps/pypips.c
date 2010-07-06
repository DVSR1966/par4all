/*

  $Id$

  Copyright 1989-2010 MINES ParisTech
  Copyright 2009-2010 TÉLÉCOM Bretagne
  Copyright 2009-2010 HPC Project

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <stdlib.h>
#include <stdio.h>

#include "linear.h"
#include "genC.h"

#include "ri.h"
#include "database.h"

#include "misc.h"

#include "ri-util.h" /* ri needed for statement_mapping in pipsdbm... */
#include "pipsdbm.h"
#include "resources.h"
#include "phases.h"
#include "properties.h"
#include "pipsmake.h"

#include "top-level.h"

void atinit()
{
    /* init various composants */
    initialize_newgen();
    initialize_sc((char*(*)(Variable))entity_local_name);
    pips_log_handler = smart_log_handler;
    set_exception_callbacks(push_pips_context, pop_pips_context);
}


void create(char* workspace_name, char ** filenames)
{
    /* create the array of arguments */
    gen_array_t filename_list = gen_array_make(0);
    while(*filenames)
    {
        gen_array_append(filename_list,*filenames);
        filenames++;
    }

    if (workspace_exists_p(workspace_name))
        pips_user_error
            ("Workspace %s already exists. Delete it!\n", workspace_name);
    else if (db_get_current_workspace_name()) {
        pips_user_error("Close current workspace %s before "
                "creating another one!\n",
                db_get_current_workspace_name());
    }
    else
    {
        if (db_create_workspace(workspace_name))
        {
            if (!create_workspace(filename_list))
            {
                db_close_workspace(false);
                pips_user_error("Could not create workspace %s\n",
                        workspace_name);
            }
        }
        else {
            pips_user_error("Cannot create directory for workspace"
                    ", check rights!\n");
        }
    }
    gen_array_free(filename_list);
}

void quit()
{
    close_workspace(FALSE);
}

void set_property(char* propname, char* value)
{
    size_t len =strlen(propname) + strlen(value) + 2;
    char * line = calloc(len,sizeof(char));
    strcat(line,propname);
    strcat(line," ");
    strcat(line,value);
    parse_properties_string(line);
    free(line);
}

char* info(char * about)
{
    string sinfo = NULL;
    if (same_string_p(about, "workspace"))
    {
        sinfo = db_get_current_workspace_name();
        if(sinfo) sinfo=strdup(sinfo);
    }
    else if (same_string_p(about, "module"))
    {
        sinfo = db_get_current_module_name();
        if(sinfo) sinfo=strdup(sinfo);
    }
    else if (same_string_p(about, "modules") && db_get_current_workspace_name())
    {
        gen_array_t modules = db_get_module_list();
        int n = gen_array_nitems(modules), i;

        size_t sinfo_size=0;
        for(i=0; i<n; i++)
        {
            string m = gen_array_item(modules, i);
            sinfo_size+=strlen(m)+1;
        }
        sinfo=strdup(string_array_join(modules, " "));
        if(!sinfo) fprintf(stderr,"not enough memory to hold all module names\n");
        gen_array_full_free(modules);
    }
    else if (same_string_p(about, "directory"))
    {
        char pathname[MAXPATHLEN];
        sinfo=getcwd(pathname, MAXPATHLEN);
        if(sinfo)
            sinfo=strdup(sinfo);
        else
            fprintf(stderr,"failer to retreive current working directory\n");
    }

    if(!sinfo)
        sinfo=strdup("");
    return sinfo;
}

void apply(char * phasename, char * target)
{
    if (!safe_apply(phasename,target)) {
        pips_user_error("error in applying transformation %s to module %s\n", phasename, target);
    }
}

void capply(char * phasename, char ** targets)
{
    /* create the array of arguments */
    gen_array_t target_list = gen_array_make(0);
    while(*targets)
    {
        gen_array_append(target_list,*targets);
        targets++;
    }
    safe_concurrent_apply(phasename,target_list);
    gen_array_free(target_list);
}

void display(char *rname, char *mname)
{
    bool has_current_module_name = db_get_current_module_name()!=NULL;
    if(has_current_module_name)
        db_reset_current_module_name();

    db_set_current_module_name(mname);
    string fname = build_view_file(rname);
    db_reset_current_module_name();

    if (!fname)
    {
        pips_user_error("Cannot build view file %s\n", rname);
        return;
    }

    if (!file_exists_p(fname))
    {
        pips_user_error("View file \"%s\" not found\n", fname);
        return;
    }

    FILE * in = safe_fopen(fname, "r");
    safe_cat(stdout, in);
    safe_fclose(in, fname);

    free(fname);
    return;
}

char* show(char * rname, char *mname)
{
    if (!db_resource_p(rname, mname)) {
        pips_user_warning("no resource %s[%s].\n", rname, mname);
        return  strdup("");
    }

    if (!displayable_file_p(rname)) {
        pips_user_warning("resource %s cannot be displayed.\n", rname);
        return strdup("");
    }

    /* now returns the name of the file.
    */
    return strdup(db_get_memory_resource(rname, mname, TRUE));
}

/* Returns the list of the modules that call that specific module,
   separated by ' '. */
char * get_callers_of(char * module_name)
{
    gen_array_t callers = get_callers(module_name);

    char * callers_string = strdup(string_array_join(callers, " "));

    gen_array_free(callers);

    return callers_string;
}
