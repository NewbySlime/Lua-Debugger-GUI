# USER DATA

MAIN_PROGRAM_FILE= TestSrc/test_dll.cpp
OUTPUT_FILE= main.exe

DLL_OUTPUT_FILE= ../../godot_workspace/bin/CPPAPI.dll
STATIC_DLIB_OUTPUT_FILE = ../../godot_workspace/bin/CPPAPI_static.lib

CLUA_SOURCE_FOLDER= LuaSrc/*.c
CPPLIB_SOURCE_FOLDER= Src/*.cpp
CPPLIB_SOURCE_STATIC_DLIB_SCRIPTS= Src/lua_vararr.cpp Src/lua_variant.cpp Src/string_store.cpp Src/stdlogger.cpp Src/luatable_util.cpp

# END OF USER DATA


AS_DLL=FALSE
AS_DEBUG=FALSE

COMPILER_TYPE=mingw


COMPILE_COMMAND=
MINGW_COMPILE_COMMAND=g++
MSVC_COMPILE_COMMAND=cl

LINK_COMMAND=
MINGW_LINK_COMMAND=g++
MSVC_LINK_COMMAND=link

COMBINING_LINK_COMMAND=
MINGW_COMBINING_LINK_COMMAND=ld
MSVC_COMBINING_LINK_COMMAND=lib


OUTPUT_TARGET_OPTION=
MINGW_OUTPUT_TARGET_OPTIONS= -o 
MSVC_OUTPUT_TARGET_OPTIONS= /OUT:

COMBINING_OUTPUT_TARGET_OPTION=
MINGW_COMBINING_OUTPUT_TARGET_OPTIONS= -o 
MSVC_COMBINING_OUTPUT_TARGET_OPTIONS= /OUT:


COMPILE_OPTION=
MINGW_COMPILE_OPTIONS= -c -std=c++17
MSVC_COMPILE_OPTIONS= /c /std:c++17 /EHsc /TP

LINK_OPTION=
MINGW_LINK_OPTIONS=
MSVC_LINK_OPTIONS= /MT

LINK_DLL_OPTION=
MINGW_LINK_DLL_OPTIONS= -shared
MSVC_LINK_DLL_OPTIONS= /DLL

COMBINE_LINK_OPTION= 
MINGW_COMBINE_LINK_OPTIONS= -r
MSVC_COMBINE_LINK_OPTIONS= 

DEBUG_COMPILE_OPTION=
MINGW_DEBUG_COMPILE_OPTIONS= -g -O0 -ggdb
MSVC_DEBUG_COMPILE_OPTIONS= /Z7

DEBUG_LINK_OPTION=
MINGW_DEBUG_LINK_OPTIONS=
MSVC_DEBUG_LINK_OPTIONS= /DEBUG


COMPILED_OBJECT_EXT=
MINGW_COMPILED_OBJECT_EXT=.o
MSVC_COMPILED_OBJECT_EXT=.obj


# MARK: Fn cmp_str
# Arg 1: String 1
# Arg 2: String 2
cmp_str=$(findstring $(1),$(2))


# MARK: Fn update_compiler_data
define update_compiler_data
$(if $(call cmp_str,$(COMPILER_TYPE),mingw),
	$(eval COMPILE_COMMAND=$(MINGW_COMPILE_COMMAND))
	$(eval COMPILE_OPTION=$(MINGW_COMPILE_OPTIONS))

	$(eval LINK_COMMAND=$(MINGW_LINK_COMMAND))
	$(eval LINK_OPTION=$(MINGW_LINK_OPTIONS))

	$(eval COMBINING_LINK_COMMAND=$(MINGW_COMBINING_LINK_COMMAND))
	$(eval COMBINE_LINK_OPTION=$(MINGW_COMBINE_LINK_OPTIONS))

	$(eval OUTPUT_TARGET_OPTION=$(MINGW_OUTPUT_TARGET_OPTIONS))
	$(eval COMPILED_OBJECT_EXT=$(MINGW_COMPILED_OBJECT_EXT))
	$(eval COMBINING_OUTPUT_TARGET_OPTION=$(MINGW_COMBINING_OUTPUT_TARGET_OPTIONS))
)

$(if $(call cmp_str,$(COMPILER_TYPE),msvc),
	$(eval COMPILE_COMMAND=$(MSVC_COMPILE_COMMAND))
	$(eval COMPILE_OPTION=$(MSVC_COMPILE_OPTIONS))

	$(eval LINK_COMMAND=$(MSVC_LINK_COMMAND))
	$(eval LINK_OPTION=$(MSVC_LINK_OPTION))

	$(eval COMBINING_LINK_COMMAND=$(MSVC_COMBINING_LINK_COMMAND))
	$(eval COMBINE_LINK_OPTION=$(MSVC_COMBINE_LINK_OPTIONS))

	$(eval COMPILED_OBJECT_EXT=$(MSVC_COMPILED_OBJECT_EXT))
	$(eval OUTPUT_TARGET_OPTION=$(MSVC_OUTPUT_TARGET_OPTIONS))
	$(eval COMBINING_OUTPUT_TARGET_OPTION=$(MSVC_COMBINING_OUTPUT_TARGET_OPTIONS))
)
endef


# MARK: Fn update_debug_flag
define update_debug_flag
$(if $(call cmp_str,$(COMPILER_TYPE),mingw),
	$(eval DEBUG_COMPILE_OPTION=$(MINGW_DEBUG_COMPILE_OPTIONS))
	$(eval DEBUG_LINK_OPTION=$(MINGW_DEBUG_LINK_OPTIONS))
)

$(if $(call cmp_str,$(COMPILER_TYPE),msvc),
	$(eval DEBUG_COMPILE_OPTION=$(MSVC_DEBUG_COMPILE_OPTIONS))
	$(eval DEBUG_LINK_OPTION=$(MSVC_DEBUG_LINK_OPTIONS))
)
endef


# MARK: Fn update_dll_flag
define update_dll_flag
$(if $(call cmp_str,$(COMPILER_TYPE),mingw),
	$(eval LINK_DLL_OPTION=$(MINGW_LINK_DLL_OPTIONS))
)

$(if $(call cmp_str,$(COMPILER_TYPE),msvc),
	$(eval LINK_DLL_OPTION=$(MSVC_LINK_DLL_OPTIONS))
)
endef


# MARK: Fn check_debug_flag
define check_debug_flag
$(if $(call cmp_str,$(AS_DEBUG),TRUE),
	$(call update_debug_flag)
)
endef


# MARK: Fn delete_objects
# Arg 1: File to delete
define delete_objects
	DEL $(1)
endef


# MARK: Fn compile_script
# Arg 1: Script to compile
define compile_script
	$(COMPILE_COMMAND) $(COMPILE_OPTION) $(1) $(DEBUG_COMPILE_OPTION)
endef


# MARK: Fn link_compilation
# Arg 1: Target compilation file
# Arg 2: Output path
# Arg 3: Additional flag
define link_compilation
	$(LINK_COMMAND) $(LINK_OPTION) $(1) $(OUTPUT_TARGET_OPTION)$(2) $(DEBUG_LINK_OPTION) $(3)
endef


# MARK: Fn combine_compilation
# Arg 1: Target compilation file
# Arg 2: Output path
define combine_compilation
	$(COMBINING_LINK_COMMAND) $(COMBINE_LINK_OPTION) $(1) $(COMBINING_OUTPUT_TARGET_OPTION)$(2)
endef




_f_update_compiler:
	$(call update_compiler_data)
	$(call update_dll_flag)
	$(call check_debug_flag)

f_use_mingw:
	$(eval COMPILER_TYPE=mingw)

f_use_msvc:
	$(eval COMPILER_TYPE=msvc)

f_as_dll:
	$(eval AS_DLL=TRUE)

f_as_debug:
	$(eval AS_DEBUG=TRUE)




define _compile_static_dlibs
$(if $(call cmp_str,$(AS_DLL),TRUE),
	$(call compile_script,$(CPPLIB_SOURCE_STATIC_DLIB_SCRIPTS))
	$(call combine_compilation,*$(COMPILED_OBJECT_EXT),$(STATIC_DLIB_OUTPUT_FILE))
)
endef

define _compile_main_program
$(if $(call cmp_str,$(AS_DLL),FALSE),
	$(call compile_script,$(MAIN_PROGRAM_FILE))
)
endef


proc_compile: _f_update_compiler
	$(call delete_objects,*$(COMPILED_OBJECT_EXT))

	$(call compile_script,$(CLUA_SOURCE_FOLDER))
	$(call _compile_static_dlibs)
	$(call compile_script,$(CPPLIB_SOURCE_FOLDER))
	$(call _compile_main_program)

	$(eval _OUTPUT_FILE=$(if $(call cmp_str,$(AS_DLL),TRUE),$(DLL_OUTPUT_FILE),$(OUTPUT_FILE)))
	$(eval _ADD_OPTION=$(if $(call cmp_str,$(AS_DLL),TRUE),$(LINK_DLL_OPTION),$(NIL_VAR)))

	$(call link_compilation,*$(COMPILED_OBJECT_EXT),$(_OUTPUT_FILE),$(_ADD_OPTION))

	$(call delete_objects,*$(COMPILED_OBJECT_EXT))

mem_test:
	drmemory -logdir ./log -quiet -ignore_kernel -- $(OUTPUT_FILE)