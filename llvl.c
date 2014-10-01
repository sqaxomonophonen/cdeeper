#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "llvl.h"
#include "a.h"

static void setup_package_path(lua_State* L)
{
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	lua_pushstring(L, ";lua/?.lua"); // TODO new location eventually? relative to game root? something?
	lua_concat(L, 2);
	lua_setfield(L, -2, "path");
	lua_pop(L, 1);
}

static void brickname_to_path(lua_State* L, const char* brickname)
{
	lua_pushstring(L, "lua/bricks/");
	lua_pushstring(L, brickname);
	lua_pushstring(L, ".lua");
	lua_concat(L, 3);
}

static void pcall(lua_State* L, int nargs, int nresults)
{
	int status = lua_pcall(L, nargs, nresults, 0);
	if (status != 0) {
		const char* err = lua_tostring(L, -1);
		switch (status) {
			case LUA_ERRRUN:
				arghf("(lua runtime error) %s", err);
				break;
			case LUA_ERRMEM:
				arghf("(lua memory error) %s", err);
				break;
			case LUA_ERRERR:
				arghf("(lua error handler error) %s", err);
				break;
			default:
				arghf("(lua unknown error) %s", err);
				break;
		}
	}
}

static void loadlua(lua_State* L, const char* name)
{
	lua_getglobal(L, "dofile");
	brickname_to_path(L, name);
	pcall(L, 1, 1);
	//if (!lua_isfunction(L, -1)) arghf("expected a function from '%s'", name);
	//pcall(L,0,1); // XXX TODO call with global state?
	//if (!lua_istable(L, -1)) arghf("expected function result from '%s' to be a table", name);
	if (!lua_istable(L, -1)) arghf("expected table from '%s'", name);
}

static void pushpromptfn(lua_State* L)
{
	lua_getglobal(L, "require");
	lua_pushstring(L, "savebrick");
	pcall(L, 1, 1);
	if (!lua_isfunction(L, -1)) arghf("expected savebrick module to return a function");
}

static void pushlsonfn(lua_State* L)
{
	lua_getglobal(L, "require");
	lua_pushstring(L, "lson");
	pcall(L, 1, 1);
	if (!lua_isfunction(L, -1)) arghf("expected LSON module to return a function");
}

static void read_vertices(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "vertices");
	if (!lua_istable(L, -1)) arghf("expected r[\"vertices\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1); // XXX renamed to lua_rawlen(L, -1) later
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"vertices\"][%d] to be a table in '%s'", i, name);

		uint32_t vi = lvl_new_vertex(lvl);
		struct vec2* v = lvl_get_vertex(lvl, vi);

		lua_rawgeti(L, -1, 1);
		v->s[0] = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_rawgeti(L, -1, 2);
		v->s[1] = lua_tonumber(L, -1);
		lua_pop(L, 1);

		//lua_pushnumber(L, vi);
		//lua_setfield(L, -2, "i");

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void read_flat(lua_State* L, struct lvl_flat* flat, const char* k)
{
	lua_getfield(L, -1, k);
	if (!lua_istable(L, -1)) arghf("expected \"%s\" to be a table in flat", k);
	lua_getfield(L, -1, "plane");
	if (!lua_istable(L, -1)) arghf("expected [\"%s\"][\"plane\"] to be a table in flat", k);
	if (lua_objlen(L, -1) != 4) arghf("expected plane to have 4 elements");
	for (int i = 0; i < 4; i++) {
		lua_rawgeti(L, -1, i+1);
		flat->plane.s[i] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	lua_pop(L, 2);
}

static void read_sectors(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "sectors");
	if (!lua_istable(L, -1)) arghf("expected r[\"sectors\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1);
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"sectors\"][%d] to be a table in '%s'", i, name);

		uint32_t seci = lvl_new_sector(lvl);
		struct lvl_sector* sec = lvl_get_sector(lvl, seci);

		read_flat(L, &sec->floor, "floor");
		read_flat(L, &sec->ceiling, "ceiling");

		//lua_pushnumber(L, seci);
		//lua_setfield(L, -2, "i");

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static void read_sidedefs(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "sidedefs");
	if (!lua_istable(L, -1)) arghf("expected r[\"sidedefs\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1);
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"sidedefs\"][%d] to be a table in '%s'", i, name);

		uint32_t sdi = lvl_new_sidedef(lvl);
		struct lvl_sidedef* sd = lvl_get_sidedef(lvl, sdi);

		lua_getfield(L, -1, "sector");
		//lua_getfield(L, -1, "i");
		sd->sector = lua_tonumber(L, -1) - 1;
		lua_pop(L, 1);
		//lua_pop(L, 2);

		//lua_pushnumber(L, sdi);
		//lua_setfield(L, -2, "i");

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}


static void read_linedefs(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "linedefs");
	if (!lua_istable(L, -1)) arghf("expected r[\"linedefs\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1);
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"linedefs\"][%d] to be a table in '%s'", i, name);

		uint32_t ldi = lvl_new_linedef(lvl);
		struct lvl_linedef* ld = lvl_get_linedef(lvl, ldi);

		lua_getfield(L, -1, "vertex0");
		//lua_getfield(L, -1, "i");
		ld->vertex0 = lua_tonumber(L, -1) - 1;
		//lua_pop(L, 2);
		lua_pop(L, 1);

		lua_getfield(L, -1, "vertex1");
		//lua_getfield(L, -1, "i");
		ld->vertex1 = lua_tonumber(L, -1) - 1;
		//lua_pop(L, 2);
		lua_pop(L, 1);

		lua_getfield(L, -1, "sidedef_left");
		if (lua_isnil(L, -1)) {
			ld->sidedef_left = -1;
			lua_pop(L, 1);
		} else {
			//lua_getfield(L, -1, "i");
			ld->sidedef_left = lua_tonumber(L, -1) - 1;
			//lua_pop(L, 2);
			lua_pop(L, 1);
		}

		lua_getfield(L, -1, "sidedef_right");
		if (lua_isnil(L, -1)) {
			ld->sidedef_right = -1;
			lua_pop(L, 1);
		} else {
			//lua_getfield(L, -1, "i");
			ld->sidedef_right = lua_tonumber(L, -1) - 1;
			//lua_pop(L, 2);
			lua_pop(L, 1);
		}

		// NOTE redundant, just adding it for completeness
		//lua_pushnumber(L, ldi);
		//lua_setfield(L, -2, "i");

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

void llvl_load(const char* name, struct lvl* lvl)
{
	lua_State* L = luaL_newstate();

	luaL_openlibs(L);
	setup_package_path(L);

	loadlua(L, name);

	read_vertices(L, lvl, name);
	read_sectors(L, lvl, name);
	read_sidedefs(L, lvl, name);
	read_linedefs(L, lvl, name);

	/*
	printf("n vertices: %d\n", lvl->n_vertices);
	printf("n linedefs: %d\n", lvl->n_linedefs);
	printf("n sidedefs: %d\n", lvl->n_sidedefs);
	printf("n sectors: %d\n", lvl->n_sectors);
	*/

	lvl_build_contours(lvl);

	lua_close(L);
}

static void write_flat(lua_State* L, struct lvl_flat* flat, const char* k)
{
	lua_getfield(L, -1, k);
	if (!lua_istable(L, -1)) arghf("expected \"%s\" to be a table in flat", k);
	lua_getfield(L, -1, "plane");
	if (!lua_istable(L, -1)) arghf("expected [\"%s\"][\"plane\"] to be a table in flat", k);
	if (lua_objlen(L, -1) != 4) arghf("expected plane to have 4 elements");
	for (int i = 0; i < 4; i++) {
		lua_pushnumber(L, flat->plane.s[i]);
		lua_rawseti(L, -2, i+1);
	}

	lua_pop(L, 2);
}


static void write_sectors(lua_State* L, struct lvl* lvl)
{
	lua_getfield(L, -1, "sectors");
	if (!lua_istable(L, -1)) arghf("expected r[\"sectors\"] to be a table");

	for (int i = 0; i < lvl->n_sectors; i++) {
		struct lvl_sector* sector = lvl_get_sector(lvl, i);
		lua_rawgeti(L, -1, i+1);
		if (!lua_istable(L, -1)) arghf("expected r[\"sectors\"][%d] to be a table", i+1);

		write_flat(L, &sector->floor, "floor");
		write_flat(L, &sector->ceiling, "ceiling");

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

void llvl_save(const char* name, struct lvl* lvl)
{
	lua_State* L = luaL_newstate();

	luaL_openlibs(L);
	setup_package_path(L);

	pushpromptfn(L);
	pushlsonfn(L);

	loadlua(L, name);

	write_sectors(L, lvl);

	lua_pushstring(L, "lson brick"); // comment

	pcall(L, 2, 1); // lsonify

	brickname_to_path(L, name);
	pcall(L, 2, 1); // prompt

	lua_close(L);
}

