#if defined __samp_map_parser_included
	#endinput
#endif
#define __samp_map_parser_included

#include <a_samp>
#include <streamer>
#include <PawnPlus>
#include <FileManager>
#include <sscanf2>

//resize STRLIB_RETURN_SIZE if strlib has been previously included. we need all the buffer we can get
#if defined STRLIB_RETURN_SIZE
	#if STRLIB_RETURN_SIZE < 256
		#undef STRLIB_RETURN_SIZE
		#define STRLIB_RETURN_SIZE 256
	#endif
#endif

#if !defined STRLIB_RETURN_SIZE
	#define STRLIB_RETURN_SIZE 256
#endif
#include <strlib>

#if !defined MAX_MAP_NAME
	#define MAX_MAP_NAME (64)
#endif

#define INVALID_MAP_ID -1

#define MAP_OBJ_EXTRA_OFFSET (0)
#define MAP_OBJ_EXTRA_MAPID (1)

#if !defined MAP_FILE_DIRECTORY
	#define MAP_FILE_DIRECTORY "./Maps/"
#endif

#if !defined MAP_FILE_EXTENSION
	#define MAP_FILE_EXTENSION ".txt"
#endif

#if !defined isnull
	#define isnull(%1) ((!(%1[0])) || (((%1[0]) == '\1') && (!(%1[1]))))
#endif

static enum _:REMOVE_DATA
{
	removemodel,
	Float:removex,
	Float:removey,
	Float:removez,
	Float:removeradius
};

static enum _:MAP_DATA
{
	MapName[MAX_MAP_NAME],
	List:MapObjects,
	List:CollisionObjects,
	List:RemovedBuildings
};

static Pool:Maps;
static bool:ProcessingMaps;

forward OnMapLoaded(mapid, const mapname[], List:objects);

public OnPlayerConnect(playerid)
{
	if(!IsPlayerNPC(playerid)) ProcessRemovedBuildings(playerid);

	#if defined MAP_OnPlayerConnect
		return MAP_OnPlayerConnect(playerid);
	#else
		return 1;
	#endif
}

stock ProcessMaps()
{
	if(!pool_valid(Maps)) Maps = pool_new();
	pool_set_ordered(Maps, true);

	if(!dir_exists(MAP_FILE_DIRECTORY))
	{
		printf("[MAP PARSER] Map directory (%s) was not found. Creating directory and cancelling loading.", MAP_FILE_DIRECTORY);
		ProcessingMaps = false;
		dir_create(MAP_FILE_DIRECTORY);
		return true;
	}

	ProcessingMaps = true;
	new InitialTicks = GetTickCount();

	//read through all map files in the directory and load them
	new dir:MapsDir = dir_open(MAP_FILE_DIRECTORY), fname[MAX_MAP_NAME], ftype;
	while(dir_list(MapsDir, fname, ftype))
	{
		if(ftype == FM_DIR) continue;
		if(strfind(fname, MAP_FILE_EXTENSION) == -1) continue;
		LoadMap(fname);
	}
	dir_close(MapsDir);

	new mapcount = pool_size(Maps);
	printf("[MAP PARSER] Successfully loaded %i %s. [%ims, %i objects, %i removed buildings]\n", mapcount, mapcount == 1 ? "map" : "maps", GetTickCount() - InitialTicks, CountMapObjects(), CountRemovedBuildings());
	ProcessingMaps = false;
	return true;
}

stock LoadMap(mapname[])
{
	if(strlen(mapname) > MAX_MAP_NAME) return printf("[MAP PARSER] Map name '%s' is too long.", mapname);

	new map[MAP_DATA], remove[REMOVE_DATA], mapindex;
	strreplace(mapname, MAP_FILE_EXTENSION, "", false, 0, -1, MAX_MAP_NAME);
	format(map[MapName], MAX_MAP_NAME, mapname);
	map[MapObjects] = list_new();
	map[CollisionObjects] = list_new();
	map[RemovedBuildings] = list_new();
	mapindex = pool_add_arr(Maps, map);

	//file variables
	new File:f = f_open(sprintf("%s%s%s", MAP_FILE_DIRECTORY, mapname, MAP_FILE_EXTENSION), "r"), templine[256];

	//object variables
	new STREAMER_TAG_OBJECT:tempobj, model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, world, interior, playerid, Float:streamdist, Float:drawdist, areaid, priority;

	//object material variables
	new matindex, matmodel, mattxd[64], mattexture[64], matcolor;

	//object material text variables
	new mattext[256], matsize, matfont[32], matfontsize, matbold, matbgcolor, matalign;

	//streamer extra ID array
	new mapobject[2];

	while(f_read(f, templine))
	{
		if(isnull(templine)) continue;

		if(templine[0] == 'r')
		{
			sscanf(templine, "{s[5]}iffff", remove[removemodel], remove[removex], remove[removey], remove[removez], remove[removeradius]);
			#if defined CA_RemoveBuilding
				CA_RemoveBuilding(remove[removemodel], remove[removex], remove[removey], remove[removez], remove[removeradius]);
			#endif
			list_add_arr(map[RemovedBuildings], remove);
			continue;
		}
		else if(templine[0] == 'm')
		{
			sscanf(templine, "{s[5]}iis[64]s[64]N(0)", matindex, matmodel, mattxd, mattexture, matcolor);
			strreplace(mattexture, "putafuckingspacehere", " ", true);
			SetDynamicObjectMaterial(tempobj, matindex, matmodel, mattxd, mattexture, matcolor);
			continue;
		}
		else if(templine[0] == 't')
		{
			sscanf(templine, "{s[5]}iis[32]iinnis[256]", matindex, matsize, matfont, matfontsize, matbold, matcolor, matbgcolor, matalign, mattext);
			strreplace(matfont, "_", " ");
			strreplace(mattext, "~N~", "\n", true);
			strreplace(mattext, "putafuckingspacehere", " ", true);
			SetDynamicObjectMaterialText(tempobj, matindex, mattext, matsize, matfont, matfontsize, matbold, matcolor, matbgcolor, matalign);
			continue;
		}
		else
		{
			sscanf(templine, "iffffffiI(-1)I(-1)F(300.0)F(300.0)I(-1)I(0)", model, x, y, z, rx, ry, rz, world, interior, playerid, streamdist, drawdist, areaid, priority);
			tempobj = CreateDynamicObject(model, x, y, z, rx, ry, rz, world, interior, playerid, streamdist, drawdist, STREAMER_TAG_AREA:areaid, priority);
			
			#if defined CA_CreateObject_DC
				list_add(map[CollisionObjects], _:CA_CreateObject_DC(model, x, y, z, rx, ry, rz, drawdist));
			#endif

			mapobject[MAP_OBJ_EXTRA_OFFSET] = -5;
			mapobject[MAP_OBJ_EXTRA_MAPID] = mapindex;
			Streamer_SetArrayData(STREAMER_TYPE_OBJECT, tempobj, E_STREAMER_EXTRA_ID, mapobject);

			list_add(map[MapObjects], _:tempobj);
		}
	}
	f_close(f);

	if(list_size(map[MapObjects]) == 0)
	{
		printf("[MAP PARSER] %s did not contain any valid objects.", mapname);
		list_delete(map[MapObjects]);
		list_delete(map[CollisionObjects]);
		list_delete(map[RemovedBuildings]);
		pool_remove(Maps, mapindex);
	}
	else
	{
		if(!ProcessingMaps)
		{
			RemoveNewBuildings(mapindex);
		}
		CallLocalFunction("OnMapLoaded", "isi", mapindex, mapname, _:map[MapObjects]);
	}
	
	return mapindex;
}

stock UnloadMap(mapid)
{
	if(!pool_has(Maps, mapid)) return true;

	new map[MAP_DATA];
	pool_get_arr(Maps, mapid, map);
	if(list_valid(map[MapObjects]))
	{
		for(new Iter:i = list_iter(map[MapObjects]); iter_inside(i); iter_erase(i))
		{
			DestroyDynamicObject(STREAMER_TAG_OBJECT:iter_get(i));
		}
	}

	#if defined CA_DestroyObject_DC
	if(list_valid(map[CollisionObjects]))
	{
		for(new Iter:i = list_iter(map[CollisionObjects]); iter_inside(i); iter_erase(i))
		{
			CA_DestroyObject_DC(iter_get(i));
		}
	}
	#endif

	if(list_valid(map[CollisionObjects])) list_delete(map[CollisionObjects]);
	if(list_valid(map[MapObjects])) list_delete(map[MapObjects]);
	if(list_valid(map[RemovedBuildings])) list_delete(map[RemovedBuildings]);
	pool_remove(Maps, mapid);
	return true;
}

stock ReprocessMaps()
{
	for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_erase(i))
	{
		iter_get_arr(i, map);
		for(new Iter:m = list_iter(map[MapObjects]); iter_inside(m); iter_move_next(m))
		{
			DestroyDynamicObject(STREAMER_TAG_OBJECT:iter_get(m));
		}
		#if defined CA_DestroyObject_DC
		for(new Iter:c = list_iter(map[CollisionObjects]); iter_inside(c); iter_erase(c))
		{
			CA_DestroyObject_DC(iter_get(c));
		}
		#endif
		
		list_delete(map[MapObjects]);
		list_delete(map[CollisionObjects]);
		list_delete(map[RemovedBuildings]);
	}

	ProcessMaps();
	return true;
}

stock GetMapName(mapid)
{
	new mapname[MAX_MAP_NAME];
	if(!pool_has(Maps, mapid))
	{
		format(mapname, sizeof(mapname), "Invalid map.");
	}
	else
	{
		new map[MAP_DATA];
		pool_get_arr(Maps, mapid, map);
		format(mapname, sizeof(mapname), map[MapName]);
	}
	return mapname;
}

stock GetMapID(const name[])
{
	if(IsNumeric(name) && pool_has(Maps, strval(name)))
	{
		return strval(name);
	}
	else
	{
		for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
		{
			iter_get_arr(i, map);
			if(!strcmp(name, map[MapName], true))
			{
				return iter_get_key(i);
			}
		}
	}
	return INVALID_MAP_ID;
}

stock GetMapIDFromObject(STREAMER_TAG_OBJECT:objectid)
{
	if(!IsValidDynamicObject(objectid)) return INVALID_MAP_ID;

	new mapobject[2];
	Streamer_GetArrayData(STREAMER_TYPE_OBJECT, objectid, E_STREAMER_EXTRA_ID, mapobject);
	if(mapobject[MAP_OBJ_EXTRA_OFFSET] == -5)
	{
		return mapobject[MAP_OBJ_EXTRA_MAPID];
	}
	return INVALID_MAP_ID;
}

stock GetMapCount()
{
	return pool_size(Maps);
}

stock GetMapList(String:string, bool:order = false)
{
	str_append_format(string, "Slot\tMap Name\tObjects\n");
	if(order)
	{
		new mapinfo[2], List:maplist = list_new();
		for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
		{
			iter_get_arr(i, map);
			mapinfo[0] = iter_get_key(i);
			mapinfo[1] = list_size(map[MapObjects]);
			list_add_arr(maplist, mapinfo);
		}

		list_sort(maplist, 1, -1, true);
		for(new Iter:i = list_iter(maplist); iter_inside(i); iter_move_next(i))
		{
			iter_get_arr(i, mapinfo);
			str_append_format(string, "%i\t%s\t%i\n", mapinfo[0], GetMapName(mapinfo[0]), mapinfo[1]);
		}
		list_delete(maplist);
	}
	else
	{
		for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
		{
			iter_get_arr(i, map);
			str_append_format(string, "%i\t%s\t%i\n", iter_get_key(i), map[MapName], list_size(map[MapObjects]));
		}
	}
	return true;
}

stock IsValidMapFile(const mapname[])
{
	return (file_exists(sprintf("%s%s%s", MAP_FILE_DIRECTORY, mapname, MAP_FILE_EXTENSION)));
}

stock ExportMap(mapid)
{
	new mapname[MAX_MAP_NAME];
	format(mapname, MAX_MAP_NAME, "%s%s", GetMapName(mapid), MAP_FILE_EXTENSION);
	if(file_exists(mapname)) file_delete(mapname);
	file_create(mapname);

	new STREAMER_TAG_OBJECT:tempobject, File:f = f_open(mapname, "w"), map[MAP_DATA], remove[REMOVE_DATA];
	pool_get_arr(Maps, mapid, map);

	f_write(f, "new tempobject;\n");

	//write removed buildings
	for(new Iter:i = list_iter(map[RemovedBuildings]); iter_inside(i); iter_move_next(i))
	{
		iter_get_arr(i, remove);
		f_write(f, sprintf("RemoveBuildingForPlayer(playerid, %i, %f, %f, %f, %.2f);\n", remove[removemodel], remove[removex], remove[removey], remove[removez], remove[removeradius]));
	}

	//write all objects
	for(new k = 0; k < 2; k++) //write objects with materials first, then objects without
	{
		for(new Iter:i = list_iter(map[MapObjects]); iter_inside(i); iter_move_next(i))
		{
			tempobject = STREAMER_TAG_OBJECT:iter_get(i);
			if(!IsValidDynamicObject(tempobject)) continue;

			new bool:matused;
			for(new idx = 0; idx < 16; idx++)
			{
				if(IsDynamicObjectMaterialUsed(tempobject, idx) || IsDynamicObjectMaterialTextUsed(tempobject, idx))
				{
					matused = true;
					break;
				}
			}

			new model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, world, interior, player, Float:streamdist, Float:drawdist;
			GetDynamicObjectPos(tempobject, x, y, z);
			GetDynamicObjectRot(tempobject, rx, ry, rz);
			model = Streamer_GetIntData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_MODEL_ID);
			world = Streamer_GetIntData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_WORLD_ID);
			interior = Streamer_GetIntData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_INTERIOR_ID);
			player = Streamer_GetIntData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_PLAYER_ID);
			Streamer_GetFloatData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_STREAM_DISTANCE, streamdist);
			Streamer_GetFloatData(STREAMER_TYPE_OBJECT, tempobject, E_STREAMER_DRAW_DISTANCE, drawdist);

			if(matused)
			{
				if(k != 0) continue;
				f_write(f, sprintf("%sCreateDynamicObject(%i, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %i, %i, %i, %.2f, %.2f);\n", matused ? "tempobject = " : "",
					model, x, y, z, rx, ry, rz, world, interior, player, streamdist, drawdist));
				for(new idx = 0; idx < 16; idx++)
				{
					if(IsDynamicObjectMaterialUsed(tempobject, idx))
					{
						new material_model, material_txd[64], material_texture[64], material_color;
						GetDynamicObjectMaterial(tempobject, idx, material_model, material_txd, material_texture, material_color);

						f_write(f, sprintf("SetDynamicObjectMaterial(tempobject, %i, %i, \"%s\", \"%s\", 0x%08x);\n", idx, material_model, material_txd, material_texture, material_color));
					}
					if(IsDynamicObjectMaterialTextUsed(tempobject, idx))
					{
						new material_text[256], material_size, material_font[32], material_fontsize, material_bold, material_fontcolor, material_bgcolor, material_textalign;
						GetDynamicObjectMaterialText(tempobject, idx, material_text, material_size, material_font, material_fontsize, material_bold, material_fontcolor, material_bgcolor, material_textalign);
						strreplace(material_text, "\n", "~N~");

						f_write(f, sprintf("SetDynamicObjectMaterialText(tempobject, %i, \"%s\", %i, \"%s\", %i, %i, 0x%08x, 0x%08x, %i);\n",
							idx, material_text, material_size, material_font, material_fontsize, material_bold, material_fontcolor, material_bgcolor, material_textalign));
					}
				}
			}
			if(k == 1) f_write(f, sprintf("%sCreateDynamicObject(%i, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %i, %i, %i, %.2f, %.2f);\n", matused ? "tempobject = " : "",
				model, x, y, z, rx, ry, rz, world, interior, player, streamdist, drawdist));
		}
	}

	f_close(f);
	return true;
}

stock MoveMapObjects(mapid, Float:xoffset, Float:yoffset, Float:zoffset)
{
	if(!pool_has(Maps, mapid)) return true;

	new map[MAP_DATA];
	pool_get_arr(Maps, mapid, map);

	if(list_valid(map[MapObjects]))
	{
		for(new Iter:i = list_iter(map[MapObjects]), Float:x, Float:y, Float:z, STREAMER_TAG_OBJECT:objectid; iter_inside(i); iter_move_next(i))
		{
			objectid = STREAMER_TAG_OBJECT:iter_get(i);
			if(!IsValidDynamicObject(objectid)) continue;

			GetDynamicObjectPos(objectid, x, y, z);
			SetDynamicObjectPos(objectid, x + xoffset, y + yoffset, z + zoffset);

			#if defined CA_SetObjectPos_DC
				CA_SetObjectPos_DC(list_get(map[CollisionObjects], i), x + xoffset, y + yoffset, z + zoffset);
			#endif
		}
	}
	return true;
}

static stock RemoveNewBuildings(mapid)
{
	if(pool_has(Maps, mapid))
	{
		new List:buildings = List:pool_get(Maps, mapid, RemovedBuildings);
		if(list_valid(buildings))
		{
			for(new Iter:i = list_iter(buildings), remove[REMOVE_DATA]; iter_inside(i); iter_move_next(i))
			{
				iter_get_arr(i, remove);
				for(new p = 0, size = GetPlayerPoolSize(); p <= size; ++p)
				{
					if(IsPlayerConnected(p))
					{
						RemoveBuildingForPlayer(p, remove[removemodel], remove[removex], remove[removey], remove[removez], remove[removeradius]);
					}
				}
			}
		}
	}
	return true;
}

static stock ProcessRemovedBuildings(playerid)
{
	for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
	{
		iter_get_arr(i, map);
		if(list_valid(map[RemovedBuildings]))
		{
			for(new Iter:b = list_iter(map[RemovedBuildings]), remove[REMOVE_DATA]; iter_inside(b); iter_move_next(b))
			{
				iter_get_arr(b, remove);
				RemoveBuildingForPlayer(playerid, remove[removemodel], remove[removex], remove[removey], remove[removez], remove[removeradius]);
			}
		}
	}
	return true;
}

static stock CountMapObjects()
{
	new count;
	for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
	{
		iter_get_arr(i, map);
		count += list_size(map[MapObjects]);
	}
	return count;
}

static stock CountRemovedBuildings()
{
	new count;
	for(new Iter:i = pool_iter(Maps), map[MAP_DATA]; iter_inside(i); iter_move_next(i))
	{
		iter_get_arr(i, map);
		if(list_valid(map[RemovedBuildings]))
		{
			count += list_size(map[RemovedBuildings]);
		}
	}
	return count;
}

//hooks
#if defined _ALS_OnPlayerConnect
	#undef OnPlayerConnect
#else
	#define _ALS_OnPlayerConnect
#endif

#define OnPlayerConnect MAP_OnPlayerConnect
#if defined MAP_OnPlayerConnect
	forward MAP_OnPlayerConnect(playerid);
#endif
