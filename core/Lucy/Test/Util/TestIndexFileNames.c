#include "Lucy/Util/ToolSet.h"

#include "Lucy/Test.h"
#include "Lucy/Test/Util/TestIndexFileNames.h"
#include "Lucy/Util/IndexFileNames.h"

static void
test_local_part(TestBatch *batch)
{
    ZombieCharBuf  source = ZCB_BLANK;
    ZombieCharBuf  target = ZCB_BLANK;
    ZombieCharBuf *got   = &target;
    
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals(got, (Obj*)&source), "simple name");

    ZCB_Assign_Str(&source, "foo.txt", 7);
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals(got, (Obj*)&source), "name with extension");

    ZCB_Assign_Str(&source, "/foo", 4);
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals_Str(got, "foo", 3), "strip leading slash");

    ZCB_Assign_Str(&source, "/foo/", 5);
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals_Str(got, "foo", 3), "strip trailing slash");

    ZCB_Assign_Str(&source, "foo/bar\\ ", 9);
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals_Str(got, "bar\\ ", 5),
        "Include garbage like backslashes and spaces");

    ZCB_Assign_Str(&source, "foo/bar/baz.txt", 15);
    got = IxFileNames_local_part((CharBuf*)&source, got);
    ASSERT_TRUE(batch, ZCB_Equals_Str(got, "baz.txt", 7), "find last component");
}

static void
test_extract_gen(TestBatch *batch)
{
    ZombieCharBuf source = ZCB_BLANK;

    source = ZCB_make_str("seg_9", 5);
    ASSERT_TRUE(batch, IxFileNames_extract_gen((CharBuf*)&source) == 9, 
        "extract_gen");

    source = ZCB_make_str("seg_9/", 6);
    ASSERT_TRUE(batch, IxFileNames_extract_gen((CharBuf*)&source) == 9, 
        "deal with trailing slash");

    source = ZCB_make_str("seg_9_8", 7);
    ASSERT_TRUE(batch, IxFileNames_extract_gen((CharBuf*)&source) == 9, 
        "Only go past first underscore");

    source = ZCB_make_str("snapshot_5.json", 15);
    ASSERT_TRUE(batch, IxFileNames_extract_gen((CharBuf*)&source) == 5, 
        "Deal with file suffix");
}

void
TestIxFileNames_run_tests()
{
    TestBatch *batch = Test_new_batch("TestIndexFileNames", 10, NULL);

    PLAN(batch);

    test_local_part(batch);
    test_extract_gen(batch);

    batch->destroy(batch);
}

/* Copyright 2009 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

