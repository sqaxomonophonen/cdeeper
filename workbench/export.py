import bpy
from bpy_extras import mesh_utils
from mathutils import Vector, Matrix
import sys
import os
import struct
import math

source_name = None
destination_name = None
basename = None
for arg in sys.argv:
	name, ext = os.path.splitext(arg)
	if ext == ".blend":
		source_name = arg
		destination_name = "%s.msh" % name
		basename = os.path.basename(name)
		break

if destination_name is None:
	raise RuntimeError("no .blend")

print("exporting %s -> %s" % (source_name, destination_name))

for bo in bpy.data.objects:
	if bo.type == "MESH" and bo.name == basename:
		tx = Matrix.Scale(16, 4) * Matrix.Rotation(-math.pi/2,4,"X")
		vertices = []
		indices = []
		mesh = bo.to_mesh(bpy.context.scene, True, "PREVIEW")
		for polygon in mesh.polygons:
			pvs = []
			for i in range(polygon.loop_start, polygon.loop_start + polygon.loop_total):
				co = tx * bo.matrix_world * mesh.vertices[mesh.loops[i].vertex_index].co
				uv = mesh.uv_layers.active.data[i].uv
				data = list(co) + list(uv)
				pvs.append(data)
			ofz = len(vertices)
			for t in range(len(pvs)-2):
				proto = []
				for u in range(3):
					proto.append(ofz + u + (u>0 and t or 0))
				proto.reverse()
				indices.extend(proto)
			vertices += pvs

		print("%d vertices" % len(vertices))
		print("%d triangles" % (len(indices)/3))

		bin = b""

		bin += struct.pack("i", len(vertices))
		for v in vertices:
			bin += struct.pack("fffff", *v)

		bin += struct.pack("i", len(indices))
		for i in indices:
			bin += struct.pack("i", i)

		with open(destination_name, "wb") as f:
			f.write(bin)
