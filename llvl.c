#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "llvl.h"
#include "a.h"
#include "m.h"
#include "names.h"

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

static void load_lua_brick(lua_State* L, const char* name)
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

static void read_vec2(lua_State* L, struct vec2* v)
{
	lua_rawgeti(L, -1, 1);
	v->s[0] = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_rawgeti(L, -1, 2);
	v->s[1] = lua_tointeger(L, -1);
	lua_pop(L, 1);
}

static void read_vertices(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "vertices");
	if (!lua_istable(L, -1)) arghf("expected r[\"vertices\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1); // XXX renamed to lua_rawlen(L, -1) later
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"vertices\"][%d] to be a table in '%s'", i, name);
		read_vec2(L, lvl_get_vertex(lvl, lvl_new_vertex(lvl)));
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void read_tx(lua_State* L, struct mat23* tx)
{
	if (!lua_istable(L, -1)) arghf("expected tx to be a table");
	if (lua_objlen(L, -1) != 6) arghf("expected tx to have 6 elements");
	for (int i = 0; i < 6; i++) {
		lua_rawgeti(L, -1, i+1);
		tx->s[i] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
}

static void read_flat(lua_State* L, struct lvl_sector* sector, int index)
{
	struct lvl_flat* flat = &sector->flat[index];

	lua_getfield(L, -1, "flat");
	if (!lua_istable(L, -1)) arghf("expected \"flat\" to be a table");
	lua_rawgeti(L, -1, index+1);
	if (!lua_istable(L, -1)) arghf("expected flat[%d] to be a table", index+1);

	lua_getfield(L, -1, "z");
	flat->z = lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, -1, "texture");
	if (lua_isnil(L, -1)) {
		flat->texture = 0;
	} else {
		flat->texture = names_find_flat(lua_tostring(L, -1));
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "tx");
	if (lua_isnil(L, -1)) {
		mat23_set_identity(&flat->tx);
	} else {
		read_tx(L, &flat->tx);
	}
	lua_pop(L, 1);

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

		for (int i = 0; i < 2; i++) {
			read_flat(L, sec, i);
		}

		lua_getfield(L, -1, "light_level");
		if (lua_isnil(L, -1)) {
			sec->light_level = 1.0;
		} else {
			sec->light_level = lua_tonumber(L, -1);
		}
		lua_pop(L, 1);

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
		sd->sector = lua_tointeger(L, -1) - 1;
		lua_pop(L, 1);

		lua_getfield(L, -1, "texture");
		if (lua_isnil(L, -1)) {
			sd->texture[0] = sd->texture[1] = 0;
		} else {
			if (!lua_istable(L, -1)) arghf("expected r[\"sidedefs\"][%d][\"texture\"] to be a table in '%s'", i, name);
			if (lua_objlen(L, -1) != 2) arghf("expected r[\"sidedefs\"][%d][\"texture\"] to be a table with two elements in '%s'", i, name);
			for (int j = 0; j < 2; j++) {
				lua_rawgeti(L, -1, j+1);
				if (!lua_isstring(L, -1)) arghf("expected r[\"sidedefs\"][%d][\"texture\"][%d] to be a string in '%s'", i, j+1, name);
				sd->texture[j] = names_find_wall(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "tx");
		if (lua_isnil(L, -1)) {
			for (int j = 0; j < 2; j++) {
				mat23_set_identity(&sd->tx[j]);
			}
		} else {
			for (int j = 0; j < 2; j++) {
				lua_rawgeti(L, -1, j+1);
				read_tx(L, &sd->tx[j]);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

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

		struct lvl_linedef* ld = lvl_get_linedef(lvl, lvl_new_linedef(lvl));

		lua_getfield(L, -1, "vertex");
		if (!lua_istable(L, -1)) arghf("expected r[\"linedefs\"][%d][\"vertex\"] to be a table in '%s'", i, name);
		for (int j = 0; j < 2; j++) {
			lua_rawgeti(L, -1, j+1);
			ld->vertex[j] = lua_tointeger(L, -1) - 1;
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "sidedef");
		if (!lua_istable(L, -1)) arghf("expected r[\"linedefs\"][%d][\"sidedef\"] to be a table in '%s'", i, name);
		for (int j = 0; j < 2; j++) {
			lua_rawgeti(L, -1, j+1);
			if (lua_isnil(L, -1)) {
				ld->sidedef[j] = -1;
			} else {
				ld->sidedef[j] = lua_tointeger(L, -1) - 1;
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static void read_entities(lua_State* L, struct lvl* lvl, const char* name)
{
	lua_getfield(L, -1, "entities");
	if (!lua_istable(L, -1)) arghf("expected r[\"entities\"] to be a table in '%s'", name);

	int N = lua_objlen(L, -1);
	for (int i = 1; i <= N; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) arghf("expected r[\"entities\"][%d] to be a table in '%s'", i, name);

		struct lvl_entity* e = lvl_get_entity(lvl, lvl_new_entity(lvl));

		lua_getfield(L, -1, "type");
		e->type = names_find_entity_type(lua_tostring(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "position");
		read_vec2(L, &e->position);
		lua_pop(L, 1);

		lua_getfield(L, -1, "yaw");
		e->yaw = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void read_lvl(lua_State* L, struct lvl* lvl, const char* name)
{
	read_vertices(L, lvl, name);
	read_sectors(L, lvl, name);
	read_sidedefs(L, lvl, name);
	read_linedefs(L, lvl, name);
	read_entities(L, lvl, name);
}

void llvl_load(const char* name, struct lvl* lvl)
{
	lua_State* L = luaL_newstate();

	luaL_openlibs(L);
	setup_package_path(L);

	load_lua_brick(L, name);

	read_lvl(L, lvl, name);

	#if 0
	printf("n vertices: %d\n", lvl->n_vertices);
	printf("n linedefs: %d\n", lvl->n_linedefs);
	printf("n sidedefs: %d\n", lvl->n_sidedefs);
	printf("n sectors: %d\n", lvl->n_sectors);
	#endif

	lvl_build_contours(lvl);

	lua_close(L);
}

void llvl_build(const char* plan, struct lvl* lvl)
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	setup_package_path(L);

	lua_getglobal(L, "require");
	lua_pushstring(L, "build");
	pcall(L, 1, 1);
	if (!lua_isfunction(L, -1)) arghf("expected require('build') to yield a function");

	lua_pushstring(L, plan);
	pcall(L, 1, 1);
	if (!lua_istable(L, -1)) arghf("expected require('build')(\"%s\") to yield a table", plan);

	read_lvl(L, lvl, plan);

	lvl_build_contours(lvl);
	lua_close(L);
}

static void push_tx(lua_State* L, struct mat23* tx)
{
	lua_newtable(L);
	for (int i = 0; i < 6; i++) {
		lua_pushnumber(L, tx->s[i]);
		lua_rawseti(L, -2, i+1);
	}
}

static void write_flat(lua_State* L, struct lvl_sector* sector, int index)
{
	struct lvl_flat* flat = &sector->flat[index];

	lua_getfield(L, -1, "flat");
	if (!lua_istable(L, -1)) arghf("expected [\"flat\"] to be a table");
	lua_rawgeti(L, -1, index+1);
	if (!lua_istable(L, -1)) arghf("expected [\"flat\"][%d] to be a table", index+1);

	lua_pushnumber(L, flat->z);
	lua_setfield(L, -2, "z");

	lua_pushstring(L, names_flats[clampi(flat->texture, 0, names_number_of_flats()-1)]);
	lua_setfield(L, -2, "texture");

	push_tx(L, &flat->tx);
	lua_setfield(L, -2, "tx");

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

		for (int i = 0; i < 2; i++) write_flat(L, sector, i);

		lua_pushnumber(L, sector->light_level);
		lua_setfield(L, -2, "light_level");

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static void write_sidedefs(lua_State* L, struct lvl* lvl)
{
	lua_getfield(L, -1, "sidedefs");
	if (!lua_istable(L, -1)) arghf("expected r[\"sidedefs\"] to be a table");

	for (int i = 0; i < lvl->n_sidedefs; i++) {
		struct lvl_sidedef* sidedef = lvl_get_sidedef(lvl, i);
		lua_rawgeti(L, -1, i+1);
		if (!lua_istable(L, -1)) arghf("expected r[\"sidedefs\"][%d] to be a table", i+1);

		lua_newtable(L);
		for (int j = 0; j < 2; j++) {
			lua_pushstring(L, names_walls[clampi(sidedef->texture[j], 0, names_number_of_walls()-1)]);
			lua_rawseti(L, -2, j+1);
		}
		lua_setfield(L, -2, "texture");

		lua_newtable(L);
		for (int j = 0; j < 2; j++) {
			push_tx(L, &sidedef->tx[j]);
			lua_rawseti(L, -2, j+1);
		}
		lua_setfield(L, -2, "tx");

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

	load_lua_brick(L, name);

	write_sectors(L, lvl);
	write_sidedefs(L, lvl);

	lua_pushstring(L, "lson brick"); // comment

	pcall(L, 2, 1); // lsonify

	brickname_to_path(L, name);
	pcall(L, 2, 1); // prompt

	lua_close(L);
}

