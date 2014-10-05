#ifndef MAGIC_H
#define MAGIC_H

#define MAGIC_RWIDTH (16*24)
#define MAGIC_RHEIGHT (9*24)

// if you change this value, update it in the Makefile as well
#define MAGIC_NUM_LIGHT_LEVELS (16)

#define MAGIC_FLAT_SIZE_EXP (6)
#define MAGIC_FLAT_SIZE (1 << (MAGIC_FLAT_SIZE_EXP))

#define MAGIC_FLAT_ATLAS_SIZE_EXP (10)
#define MAGIC_FLAT_ATLAS_SIZE (1 << (MAGIC_FLAT_ATLAS_SIZE_EXP))

#define INNER_STR_VALUE(arg)	#arg
#define STR_VALUE(arg)	INNER_STR_VALUE(arg)

#endif/*MAGIC_H*/
