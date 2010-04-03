package Lucy::Object::VArray;
use Lucy;

1;

__END__

__BINDING__

my $xs_code = <<'END_XS_CODE';
MODULE = Lucy   PACKAGE = Lucy::Object::VArray

SV*
shallow_copy(self)
    lucy_VArray *self;
CODE:
    RETVAL = LUCY_OBJ_TO_SV_NOINC(Lucy_VA_Shallow_Copy(self));
OUTPUT: RETVAL

SV*
_deserialize(either_sv, instream)
    SV *either_sv;
    lucy_InStream *instream;
CODE:
    CHY_UNUSED_VAR(either_sv);
    RETVAL = LUCY_OBJ_TO_SV_NOINC(lucy_VA_deserialize(NULL, instream));
OUTPUT: RETVAL

SV*
_clone(self)
    lucy_VArray *self;
CODE:
    RETVAL = LUCY_OBJ_TO_SV_NOINC(Lucy_VA_Clone(self));
OUTPUT: RETVAL

SV*
shift(self)
    lucy_VArray *self;
CODE:
    RETVAL = LUCY_OBJ_TO_SV_NOINC(Lucy_VA_Shift(self));
OUTPUT: RETVAL

SV*
pop(self)
    lucy_VArray *self;
CODE:
    RETVAL = LUCY_OBJ_TO_SV_NOINC(Lucy_VA_Pop(self));
OUTPUT: RETVAL

SV*
delete(self, tick)
    lucy_VArray *self;
    uint32_t    tick;
CODE:
    RETVAL = LUCY_OBJ_TO_SV_NOINC(Lucy_VA_Delete(self, tick));
OUTPUT: RETVAL

void
store(self, tick, value);
    lucy_VArray *self; 
    uint32_t     tick;
    lucy_Obj    *value;
PPCODE:
{
    if (value) { LUCY_INCREF(value); }
    lucy_VA_store(self, tick, value);
}

SV*
fetch(self, tick)
    lucy_VArray *self;
    uint32_t     tick;
CODE:
    RETVAL = LUCY_OBJ_TO_SV(Lucy_VA_Fetch(self, tick));
OUTPUT: RETVAL
END_XS_CODE

Clownfish::Binding::Perl::Class->register(
    parcel       => "Lucy",
    class_name   => "Lucy::Object::VArray",
    xs_code      => $xs_code,
    bind_methods => [
        qw(
            Push
            Push_VArray
            Unshift
            Splice
            Resize
            Get_Size
            )
    ],
    bind_constructors => ["new"],
);

__COPYRIGHT__

    /**
     * Copyright 2009 The Apache Software Foundation
     *
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     *     http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
     * implied.  See the License for the specific language governing
     * permissions and limitations under the License.
     */

