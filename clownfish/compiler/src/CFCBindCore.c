/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdio.h>

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCBindCore.h"
#include "CFCBindAliases.h"
#include "CFCBindClass.h"
#include "CFCBindFile.h"
#include "CFCClass.h"
#include "CFCFile.h"
#include "CFCHierarchy.h"
#include "CFCParcel.h"
#include "CFCUtil.h"

struct CFCBindCore {
    CFCBase base;
    CFCHierarchy *hierarchy;
    char         *header;
    char         *footer;
};

/* Write the "parcel.h" header file, which contains common symbols needed by
 * all classes, plus typedefs for all class structs.
 */
static void
S_write_parcel_h(CFCBindCore *self);

/* Write the "parcel.c" file containing autogenerated implementation code.
 */
static void
S_write_parcel_c(CFCBindCore *self);

const static CFCMeta CFCBINDCORE_META = {
    "Clownfish::CFC::Binding::Core",
    sizeof(CFCBindCore),
    (CFCBase_destroy_t)CFCBindCore_destroy
};

CFCBindCore*
CFCBindCore_new(CFCHierarchy *hierarchy, const char *header,
                const char *footer) {
    CFCBindCore *self = (CFCBindCore*)CFCBase_allocate(&CFCBINDCORE_META);
    return CFCBindCore_init(self, hierarchy, header, footer);
}

CFCBindCore*
CFCBindCore_init(CFCBindCore *self, CFCHierarchy *hierarchy,
                 const char *header, const char *footer) {
    CFCUTIL_NULL_CHECK(hierarchy);
    CFCUTIL_NULL_CHECK(header);
    CFCUTIL_NULL_CHECK(footer);
    self->hierarchy = (CFCHierarchy*)CFCBase_incref((CFCBase*)hierarchy);
    self->header    = CFCUtil_strdup(header);
    self->footer    = CFCUtil_strdup(footer);
    return self;
}

void
CFCBindCore_destroy(CFCBindCore *self) {
    CFCBase_decref((CFCBase*)self->hierarchy);
    FREEMEM(self->header);
    FREEMEM(self->footer);
    CFCBase_destroy((CFCBase*)self);
}

int
CFCBindCore_write_all_modified(CFCBindCore *self, int modified) {
    CFCHierarchy *hierarchy = self->hierarchy;
    const char   *header    = self->header;
    const char   *footer    = self->footer;

    // Discover whether files need to be regenerated.
    modified = CFCHierarchy_propagate_modified(hierarchy, modified);

    // Iterate over all File objects, writing out those which don't have
    // up-to-date auto-generated files.
    const char *inc_dest = CFCHierarchy_get_include_dest(hierarchy);
    CFCFile **files = CFCHierarchy_files(hierarchy);
    for (int i = 0; files[i] != NULL; i++) {
        if (CFCFile_get_modified(files[i])) {
            CFCBindFile_write_h(files[i], inc_dest, header, footer);
        }
    }

    // If any class definition has changed, rewrite the parcel.h and parcel.c
    // files.
    if (modified) {
        S_write_parcel_h(self);
        S_write_parcel_c(self);
    }

    return modified;
}

/* Write the "parcel.h" header file, which contains common symbols needed by
 * all classes, plus typedefs for all class structs.
 */
static void
S_write_parcel_h(CFCBindCore *self) {
    CFCHierarchy *hierarchy = self->hierarchy;
    CFCParcel    *parcel    = NULL;

    // Declare object structs for all instantiable classes.
    // Obtain parcel prefix for use in bootstrap function name.
    char *typedefs = CFCUtil_strdup("");
    CFCClass **ordered = CFCHierarchy_ordered_classes(hierarchy);
    for (int i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (!CFCClass_inert(klass)) {
            const char *full_struct = CFCClass_full_struct_sym(klass);
            typedefs = CFCUtil_cat(typedefs, "typedef struct ", full_struct,
                                   " ", full_struct, ";\n", NULL);
        }
        if (!CFCClass_included(klass)) {
            if (parcel && CFCClass_get_parcel(klass) != parcel) {
                CFCUtil_die("Multiple parcels not yet supported.");
            }
            parcel = CFCClass_get_parcel(klass);
        }
    }
    if (!parcel) {
        CFCUtil_die("No source classes found.");
    }
    const char *prefix = CFCParcel_get_prefix(parcel);
    FREEMEM(ordered);

    // Create Clownfish aliases if necessary.
    char *aliases = CFCBindAliases_c_aliases();

    const char *visibility = strcmp(prefix, "lucy_") == 0
                             ? "CHY_EXPORT" : "CHY_IMPORT";

    const char pattern[] =
        "%s\n"
        "#ifndef CFCPARCEL_H\n"
        "#define CFCPARCEL_H 1\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "#include <stddef.h>\n"
        "#include \"charmony.h\"\n"
        "\n"
        "%s\n"
        "%s\n"
        "\n"
        "/* Refcount / host object */\n"
        "typedef union {\n"
        "    size_t  count;\n"
        "    void   *host_obj;\n"
        "} cfish_ref_t;\n"
        "\n"
        "/* Generic method pointer.\n"
        " */\n"
        "typedef void\n"
        "(*cfish_method_t)(const void *vself);\n"
        "\n"
        "/* Access the function pointer for a given method from the vtable.\n"
        " */\n"
        "#define CFISH_METHOD_PTR(_vtable, _full_meth) \\\n"
        "     ((_full_meth ## _t)cfish_method(_vtable, _full_meth ## _OFFSET))\n"
        "\n"
        "static CHY_INLINE cfish_method_t\n"
        "cfish_method(const void *vtable, size_t offset) {\n"
        "    union { char *cptr; cfish_method_t *fptr; } ptr;\n"
        "    ptr.cptr = (char*)vtable + offset;\n"
        "    return ptr.fptr[0];\n"
        "}\n"
        "\n"
        "/* Access the function pointer for the given method in the superclass's\n"
        " * vtable. */\n"
        "#define CFISH_SUPER_METHOD_PTR(_vtable, _full_meth) \\\n"
        "     ((_full_meth ## _t)cfish_super_method(_vtable, \\\n"
        "                                           _full_meth ## _OFFSET))\n"
        "\n"
        "extern %s size_t cfish_VTable_offset_of_parent;\n"
        "static CHY_INLINE cfish_method_t\n"
        "cfish_super_method(const void *vtable, size_t offset) {\n"
        "    char *vt_as_char = (char*)vtable;\n"
        "    cfish_VTable **parent_ptr\n"
        "        = (cfish_VTable**)(vt_as_char + cfish_VTable_offset_of_parent);\n"
        "    return cfish_method(*parent_ptr, offset);\n"
        "}\n"
        "\n"
        "/* Return a boolean indicating whether a method has been overridden.\n"
        " */\n"
        "#define CFISH_OVERRIDDEN(_self, _full_meth, _full_func) \\\n"
        "    (cfish_method(*((cfish_VTable**)_self), _full_meth ## _OFFSET )\\\n"
        "        != (cfish_method_t)_full_func)\n"
        "\n"
        "#ifdef CFISH_USE_SHORT_NAMES\n"
        "  #define METHOD_PTR               CFISH_METHOD_PTR\n"
        "  #define SUPER_METHOD_PTR         CFISH_SUPER_METHOD_PTR\n"
        "  #define OVERRIDDEN               CFISH_OVERRIDDEN\n"
        "#endif\n"
        "\n"
        "void\n"
        "%sbootstrap_parcel();\n"
        "\n"
        "void\n"
        "%sinit_parcel();\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* CFCPARCEL_H */\n"
        "\n"
        "%s\n"
        "\n";
    size_t size = sizeof(pattern)
                  + strlen(self->header)
                  + strlen(aliases)
                  + strlen(typedefs)
                  + strlen(visibility)
                  + 2 * strlen(prefix)
                  + strlen(self->footer)
                  + 100;
    char *file_content = (char*)MALLOCATE(size);
    sprintf(file_content, pattern, self->header, aliases, typedefs, visibility,
            prefix, prefix, self->footer);

    // Unlink then write file.
    const char *inc_dest = CFCHierarchy_get_include_dest(hierarchy);
    char *filepath = CFCUtil_cat(CFCUtil_strdup(""), inc_dest,
                                 CFCUTIL_PATH_SEP, "parcel.h", NULL);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(aliases);
    FREEMEM(typedefs);
    FREEMEM(file_content);
}

static void
S_write_parcel_c(CFCBindCore *self) {
    CFCHierarchy *hierarchy = self->hierarchy;
    CFCParcel    *parcel    = NULL;

    // Aggregate C code from all files.
    // Obtain parcel prefix for use in bootstrap function name.
    char *privacy_syms = CFCUtil_strdup("");
    char *includes     = CFCUtil_strdup("");
    char *c_data       = CFCUtil_strdup("");
    char *vt_specs = CFCUtil_strdup(
        "static cfish_VTableSpec vtable_specs[] = {\n");
    int num_specs = 0;
    CFCClass **ordered  = CFCHierarchy_ordered_classes(hierarchy);
    for (int i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        if (CFCClass_included(klass)) { continue; }

        CFCBindClass *class_binding = CFCBindClass_new(klass);
        char *class_c_data = CFCBindClass_to_c_data(class_binding);
        c_data = CFCUtil_cat(c_data, class_c_data, "\n", NULL);
        FREEMEM(class_c_data);
        if (!CFCClass_inert(klass)) {
            if (num_specs != 0) {
                vt_specs = CFCUtil_cat(vt_specs, ",\n", NULL);
            }
            char *vt_spec = CFCBindClass_spec_def(class_binding);
            vt_specs = CFCUtil_cat(vt_specs, vt_spec, NULL);
            FREEMEM(vt_spec);
            ++num_specs;
        }
        CFCBase_decref((CFCBase*)class_binding);
        const char *privacy_sym = CFCClass_privacy_symbol(klass);
        privacy_syms = CFCUtil_cat(privacy_syms, "#define ",
                                   privacy_sym, "\n", NULL);
        const char *include_h = CFCClass_include_h(klass);
        includes = CFCUtil_cat(includes, "#include \"", include_h,
                               "\"\n", NULL);
        if (!CFCClass_included(klass)) {
            if (parcel && CFCClass_get_parcel(klass) != parcel) {
                CFCUtil_die("Multiple parcels not yet supported.");
            }
            parcel = CFCClass_get_parcel(klass);
        }
    }
    if (!parcel) {
        CFCUtil_die("No source classes found.");
    }
    const char *prefix = CFCParcel_get_prefix(parcel);
    vt_specs = CFCUtil_cat(vt_specs, "\n};\n", NULL);
    FREEMEM(ordered);

    const char *vt_bootstrap = "";
    if (strcmp(prefix, "lucy_") == 0) {
        vt_bootstrap =
            "    /* Bootstrap VTables.\n"
            "     */\n"
            "    cfish_VTable_override(CFISH_OBJ, (cfish_method_t)lucy_Obj_destroy, Lucy_Obj_Destroy_OFFSET);\n"
            "    cfish_VTable_override(CFISH_VTABLE, (cfish_method_t)lucy_VTable_make_obj, Lucy_VTable_Make_Obj_OFFSET);\n"
            "    CFISH_CHARBUF->vtable = CFISH_VTABLE;\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_Obj_dec_refcount, Lucy_Obj_Dec_RefCount_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_Obj_inc_refcount, Lucy_Obj_Inc_RefCount_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_CB_cat_trusted_str, Lucy_CB_Cat_Trusted_Str_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_CB_clone, Lucy_CB_Clone_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_CB_destroy, Lucy_CB_Destroy_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_CB_grow, Lucy_CB_Grow_OFFSET);\n"
            "    cfish_VTable_override(CFISH_CHARBUF, (cfish_method_t)lucy_CB_vcatf, Lucy_CB_VCatF_OFFSET);\n"
            "    CFISH_VARRAY->vtable = CFISH_VTABLE;\n"
            "    cfish_VTable_override(CFISH_VARRAY, (cfish_method_t)lucy_VA_grow, Lucy_VA_Grow_OFFSET);\n"
            "    cfish_VTable_override(CFISH_VARRAY, (cfish_method_t)lucy_VA_push, Lucy_VA_Push_OFFSET);\n"
            "    CFISH_METHOD->vtable = CFISH_VTABLE;\n"
            "\n";
    }

    char pattern[] =
        "%s\n"
        "\n"
        "#define C_LUCY_VTABLE\n"
        "%s\n"
        "#include \"parcel.h\"\n"
        "#include \"Clownfish/VTable.h\"\n"
        "%s\n"
        "\n"
        "typedef struct cfish_MethodSpec {\n"
        "    int             is_novel;\n"
        "    const char     *name;\n"
        "    cfish_method_t  func;\n"
        "    cfish_method_t  callback_func;\n"
        "    size_t         *offset;\n"
        "} cfish_MethodSpec;\n"
        "\n"
        "typedef struct cfish_VTableSpec {\n"
        "    cfish_VTable     **vtable;\n"
        "    cfish_VTable     **parent;\n"
        "    const char        *name;\n"
        "    size_t             obj_alloc_size;\n"
        "    size_t             num_fresh;\n"
        "    size_t             num_novel;\n"
        "    cfish_MethodSpec  *method_specs;\n"
        "} cfish_VTableSpec;\n"
        "\n"
        "%s\n"
        "\n"
        "/* VTableSpec structs for initialization.\n"
        " */\n"
        "%s\n"
        "\n"
        "void\n"
        "%sbootstrap_parcel() {\n"
        "    int num_vtable_specs = %d;\n"
        "\n"
        "    /* Allocate memory for VTables.\n"
        "     */\n"
        "    for (int i = 0; i < num_vtable_specs; ++i) {\n"
        "        cfish_VTableSpec *spec = &vtable_specs[i];\n"
        "        cfish_VTable *parent = spec->parent ? *spec->parent : NULL;\n"
        "        *spec->vtable = cfish_VTable_allocate(parent, 0,\n"
        "                                              spec->obj_alloc_size,\n"
        "                                              spec->num_novel);\n"
        "    }\n"
        "\n"
        "%s"
        "    /* Initialize VTables.\n"
        "     */\n"
        "    for (int i = 0; i < num_vtable_specs; ++i) {\n"
        "        cfish_VTableSpec *spec = &vtable_specs[i];\n"
        "        cfish_CharBuf *cb = cfish_CB_newf(\"%%s\", spec->name);\n"
        "        cfish_VTable_init(*spec->vtable, cb);\n"
        "        CFISH_DECREF(cb);\n"
        "        for (int i = 0; i < spec->num_fresh; ++i) {\n"
        "            cfish_MethodSpec *mspec = &spec->method_specs[i];\n"
        "            if (mspec->is_novel) {\n"
        "                cb = cfish_CB_newf(\"%%s\", mspec->name);\n"
        "                cfish_VTable_add_method(*spec->vtable, cb,\n"
        "                                        mspec->callback_func,\n"
        "                                        *mspec->offset);\n"
        "                CFISH_DECREF(cb);\n"
        "            }\n"
        "            cfish_VTable_override(*spec->vtable, mspec->func,\n"
        "                                  *mspec->offset);\n"
        "        }\n"
        "    }\n"
        "\n"
        "    /* Register VTables.\n"
        "     */\n"
        "    for (int i = 0; i < num_vtable_specs; ++i) {\n"
        "        cfish_VTableSpec *spec = &vtable_specs[i];\n"
        "        cfish_VTable_add_to_registry(*spec->vtable);\n"
        "    }\n"
        "\n"
        "    %sinit_parcel();\n"
        "}\n"
        "\n"
        "%s\n";
    size_t size = sizeof(pattern)
                  + strlen(self->header)
                  + strlen(privacy_syms)
                  + strlen(includes)
                  + strlen(c_data)
                  + strlen(vt_specs)
                  + strlen(prefix)
                  + 10 // for num_specs
                  + strlen(vt_bootstrap)
                  + strlen(prefix)
                  + strlen(self->footer)
                  + 100;
    char *file_content = (char*)MALLOCATE(size);
    sprintf(file_content, pattern, self->header, privacy_syms, includes,
            c_data, vt_specs, prefix, num_specs, vt_bootstrap, prefix,
            self->footer);

    // Unlink then open file.
    const char *src_dest = CFCHierarchy_get_source_dest(hierarchy);
    char *filepath = CFCUtil_cat(CFCUtil_strdup(""), src_dest,
                                 CFCUTIL_PATH_SEP, "parcel.c", NULL);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(privacy_syms);
    FREEMEM(includes);
    FREEMEM(c_data);
    FREEMEM(vt_specs);
    FREEMEM(file_content);
}

