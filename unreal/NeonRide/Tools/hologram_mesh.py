"""Rebuild the film's jellyfish as one draw-call mesh. Distances are metres."""
from array import array
import json
from math import cos, pi, sin, sqrt, tau
import random
import struct


def build_jellyfish(path):
    positions, normals, colours, indices = array("f"), array("f"), array("f"), array("I")
    rng = random.Random(372)

    def unit(v):
        length = sqrt(sum(c*c for c in v))
        return tuple(c / max(length, 1e-8) for c in v)

    def cross(a, b):
        return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

    def tube(points, radius, tint, sides=5):
        first = len(positions) // 3
        for j, p in enumerate(points):
            before, after = points[max(0, j-1)], points[min(len(points)-1, j+1)]
            tangent = unit(tuple(b-a for a, b in zip(before, after)))
            axis = (0,1,0) if abs(tangent[2]) > 0.93 else (0,0,1)
            normal = unit(cross(tangent, axis))
            binormal = cross(tangent, normal)
            t = j / (len(points)-1)
            for k in range(sides):
                a = tau*k/sides
                n = tuple(cos(a)*u + sin(a)*v for u,v in zip(normal, binormal))
                x,y,z = (p[i] + n[i]*radius(t) for i in range(3))
                positions.extend((x,z,-y))
                normals.extend((n[0],n[2],-n[1]))
                colours.extend((*tint(t),1))
                if j:
                    i = first+(j-1)*sides+k
                    h = first+(j-1)*sides+(k+1)%sides
                    indices.extend((i,h,i+sides,h,h+sides,i+sides))

    cyan = lambda t: (0.025,0.77,0.88)
    pink = lambda t: (0.95-0.52*t,0.025,0.35+0.48*t)
    def dome(u, angle):
        r = 30 * sin(u*pi/2) * (1 + (0.08 + 0.027*cos(16*angle))*u**4)
        z = 20.4*cos(u*pi/2) - (1.3 + 0.85*cos(32*angle))*u**6
        return (r*cos(angle),r*sin(angle),z)

    for ring in range(1,10):
        tube([dome(ring/9,tau*i/160) for i in range(161)],
             lambda t: 0.13 if ring == 9 else 0.075, cyan)
    for rib in range(48):
        tube([dome(j/22,tau*rib/48) for j in range(23)],lambda t: 0.055+0.035*t,cyan)
    for k in range(4):
        angle = k*pi/2+pi/4
        loop = []
        for j in range(35):
            a = angle-1.35+2.7*j/34
            loop.append((8*cos(angle)+4.5*cos(a),8*sin(angle)+4.5*sin(a),5.5+1.6*sin(pi*j/34)))
        tube(loop, lambda t: 0.44, pink, 8)
        points = []
        for j in range(42):
            t = j/41
            a = angle + 2.9*t + 0.16*sin(t*22)
            r = 5.4 + 2.0*sin(t*19+k) + 3*t
            points.append((r*cos(a)+3*t,r*sin(a),2-63*t))
        tube(points,lambda t: 0.85*(1-t)**0.7+0.04,pink,8)
    for i in range(64):
        a = tau*i/64+rng.uniform(-0.022,0.022)
        length = rng.uniform(26,47) if i%4 == 0 else rng.uniform(53,96)
        phase = rng.uniform(0,tau)
        x,y,z = dome(1,a)
        pts = []
        for j in range(24):
            t = j/23
            curl = 0.38*t*sin(5*t+phase)
            r = sqrt(x*x+y*y)*(1-0.56*t) + 2.3*t*sin(9*t+phase)
            pts.append((r*cos(a+curl)+6*t*t,r*sin(a+curl)+3*t*sin(4*t),z-length*t))
        tube(pts,lambda t: 0.13*(1-0.62*t),lambda t: (0.03+0.23*t*t,0.76*(1-t)**1.3,0.91-0.20*t))

    views, accessors, binary = [], [], bytearray()
    def pack(data, components, kind, datatype, bounds=False):
        offset = len(binary)
        binary.extend(data.tobytes())
        views.append(dict(buffer=0,byteOffset=offset,byteLength=len(binary)-offset))
        acc = dict(bufferView=len(views)-1,componentType=datatype,count=len(data)//components,type=kind)
        if bounds:
            acc.update(min=[min(data[i::components]) for i in range(components)],
                       max=[max(data[i::components]) for i in range(components)])
        accessors.append(acc)
        return len(accessors)-1

    attrs = {"POSITION":pack(positions,3,"VEC3",5126,True),"NORMAL":pack(normals,3,"VEC3",5126),
             "COLOR_0":pack(colours,4,"VEC4",5126)}
    idx = pack(indices,1,"SCALAR",5125)
    doc = dict(asset={"version":"2.0"},scene=0,scenes=[{"nodes":[0]}],nodes=[{"mesh":0}],
               meshes=[{"name":"SM_Hologram","primitives":[{"attributes":attrs,"indices":idx}]}],
               buffers=[{"byteLength":len(binary)}],bufferViews=views,accessors=accessors)
    header = json.dumps(doc,separators=(",",":")).encode()
    header += b" " * (-len(header)%4)
    path.write_bytes(struct.pack("<4sII",b"glTF",2,28+len(header)+len(binary))+
                     struct.pack("<I4s",len(header),b"JSON")+header+
                     struct.pack("<I4s",len(binary),b"BIN\x00")+binary)
