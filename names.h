#ifndef NAMES_H
#define NAMES_H

int names_find_flat(const char* name);
int names_number_of_flats();
extern const char* names_flats[];

int names_find_wall(const char* name);
int names_number_of_walls();
extern const char* names_walls[];


int names_find_sprite(const char* name);
int names_number_of_psrites();
extern const char* names_sprites[];


int names_find_entity_type(const char* type);

#endif/*NAMES_H*/
