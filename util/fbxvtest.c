/* Copyright (C)2009 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fbxv.h"
#include "rrtimer.h"

#define WIDTH 1241
#define HEIGHT 903

#define _throw(m) { \
	printf("ERROR: %s\n", m);  goto bailout; \
}

#define fbxv(f) \
	if((f)==-1) { \
		printf("FBXV ERROR in line %d: %s\n", fbxv_geterrline(), fbxv_geterrmsg()); \
		goto bailout; \
	}

double testtime=5.;
Display *dpy=NULL;  Window win=0;
fbxv_struct s;
int width=WIDTH, height=HEIGHT, useshm=1, scale=1, interactive=0;
char *filename=NULL;
Atom protoatom=0, deleteatom=0;

void initbuf(fbxv_struct *s, int id, int seed)
{
	int i, j, c;
	int yindex0, yindex1, uindex, vindex;

	switch(id)
	{
		case YUY2_PACKED:
			yindex0=0;  uindex=1;  yindex1=2;  vindex=3;  break;
		case UYVY_PACKED:
			uindex=0;  yindex0=1;  vindex=2;  yindex1=3;  break;
		default:
			yindex0=yindex1=uindex=vindex=0;  break;
	}
	if(id==YUY2_PACKED || id==UYVY_PACKED)
	{
		for(i=0; i<s->xvi->height; i++)
		{
			for(j=0; j<s->xvi->width; j+=2)
			{
				s->xvi->data[s->xvi->pitches[0]*i + j*2 + yindex0] = (i+seed)*(j+seed);
				s->xvi->data[s->xvi->pitches[0]*i + j*2 + yindex1] = (i+seed+1)*(j+seed+1);
				s->xvi->data[s->xvi->pitches[0]*i + j*2 + uindex] = (i+seed)*(j+seed);
				s->xvi->data[s->xvi->pitches[0]*i + j*2 + vindex] = (i+seed)*(j+seed);
			}
		}
	}
	else
	{
		for(c=0; c<s->xvi->num_planes; c++)
			for(i=0; i<s->xvi->height; i++)
				for(j=0; j<s->xvi->width; j++)
					s->xvi->data[s->xvi->offsets[c] + s->xvi->pitches[c]*(c==0? i:i/2)
						+ (c==0? j:j/2)] = (i+seed)*(j+seed);
	}
}

void dotest(int id, char *name)
{
	int iter=0, i;
	double elapsed=0., t;
	FILE *file=NULL;

	printf("Testing %s format (ID=0x%.8x) ...\n", name, id);

	memset(&s, 0, sizeof(s));
	fbxv(fbxv_init(&s, dpy, win, width/scale, height/scale, id, useshm));
	printf("Image:\n");
	printf("  Data size:   %d\n", s.xvi->data_size);
	printf("  Dimensions:  %d x %d\n", s.xvi->width, s.xvi->height);
	printf("  Planes:      %d\n", s.xvi->num_planes);
	printf("  Pitches:     [");
	for(i=0; i<s.xvi->num_planes; i++)
	{
		printf("%d", s.xvi->pitches[i]);
		if(i!=s.xvi->num_planes-1) printf(", ");
	}
	printf("]\n");
	printf("  Offsets:     [");
	for(i=0; i<s.xvi->num_planes; i++)
	{
		printf("%d", s.xvi->offsets[i]);
		if(i!=s.xvi->num_planes-1) printf(", ");
	}
	printf("]\n");
	printf("XV Port = %d\n", s.port);

	if(filename)
	{
		file=fopen(filename, "rb");
		if(!file)
		{
			printf("Could not open %s\n", filename);
			exit(-1);
		}
		if(fread(s.xvi->data, s.xvi->data_size, 1, file)!=1)
		{
			printf("Could not read %s\n", filename);
			exit(-1);
		}
		fclose(file);
	}

	do
	{
		if(!filename) initbuf(&s, id, iter);
		t=rrtime();
		fbxv(fbxv_write(&s, 0, 0, 0, 0, 0, 0, width, height));
		elapsed+=rrtime()-t;
		iter++;
	} while(elapsed<testtime);
	printf("%f Mpixels/sec\n", (double)(width*height)/1000000.*(double)iter/testtime);

	bailout:
	fbxv_term(&s);
}

void dotesti(Display *dpy, int id, char *name)
{
	int iter=0;
	printf("Interactive test, %s format (ID=0x%.8x) ...\n", name, id);
	memset(&s, 0, sizeof(s));

	while (1)
	{
		int dodisplay=0;

		while(1)
		{
			XEvent event;
			XNextEvent(dpy, &event);
			switch (event.type)
			{
				case Expose:
					dodisplay=1;
					break;
				case ConfigureNotify:
					width=event.xconfigure.width;  height=event.xconfigure.height;
					break;
				case KeyPress:
				{
					char buf[10];  int key;
					key=XLookupString(&event.xkey, buf, sizeof(buf), NULL, NULL);
					switch(buf[0])
					{
						case 27: case 'q': case 'Q':
							return;
					}
					break;
				}
				case MotionNotify:
					if(event.xmotion.state & Button1Mask) dodisplay=1;
 					break;
				case ClientMessage:
				{
					XClientMessageEvent *cme=(XClientMessageEvent *)&event;
					if(cme->message_type==protoatom && cme->data.l[0]==deleteatom)
						return;
				}
			}
			if(XPending(dpy)<=0) break;
		}
		if(dodisplay) 
		{
			fbxv(fbxv_init(&s, dpy, win, width/scale, height/scale, id, useshm));
			initbuf(&s, id, iter);  iter++;
			fbxv(fbxv_write(&s, 0, 0, 0, 0, 0, 0, width, height));
		}
	}

	bailout:
	return;
}

int main(int argc, char **argv)
{
	int id=I420_PLANAR, n=0, i;
	XVisualInfo vtemp, *v=NULL;
	XSetWindowAttributes swa;

	if(argc>1)
	{
		for(i=0; i<argc; i++)
		{
			if(!strcasecmp(argv[i], "-noshm")) useshm=0;
			else if(!strcasecmp(argv[i], "-i")) interactive=1;
			else if(!strcasecmp(argv[i], "-scale") && i<argc-1)
			{
				scale=atoi(argv[++i]);
				if(scale<1) scale=1;
			}
			else if(!strcasecmp(argv[i], "-width") && i<argc-1)
			{
				int temp=atoi(argv[++i]);
				if(temp>0) width=temp;
			}
			else if(!strcasecmp(argv[i], "-height") && i<argc-1)
			{
				int temp=atoi(argv[++i]);
				if(temp>0) height=temp;
			}
			else if(!strcasecmp(argv[i], "-file") && i<argc-1)
				filename=argv[++i];
			else if(!strcasecmp(argv[i], "-time") && i<argc-1)
			{
				double temp=atof(argv[++i]);
				if(temp>0.) testtime=temp;
			}
			else if(!strcasecmp(argv[i], "-format") && i<argc-1)
			{
				char *temp=argv[++i];
				if(!strcasecmp(temp, "yuy2")) id=YUY2_PACKED;
				else if(!strcasecmp(temp, "yv12")) id=YV12_PLANAR;
				else if(!strcasecmp(temp, "uyvy")) id=UYVY_PACKED;
				else if(!strcasecmp(temp, "i420")) id=I420_PLANAR;
			}
		}
	}

	fbxv_printwarnings(stdout);

	if(!(dpy=XOpenDisplay(NULL))) _throw("Could not open display");

	vtemp.depth=24;  vtemp.class=TrueColor;
	if((v=XGetVisualInfo(dpy, VisualDepthMask|VisualClassMask, &vtemp, &n))==NULL
		|| n==0)
		_throw("Could not obtain a TrueColor visual");
	swa.colormap=XCreateColormap(dpy, DefaultRootWindow(dpy), v->visual,
		AllocNone);
	swa.border_pixel=0;
	swa.background_pixel=0;
	swa.event_mask=0;
	if(interactive) swa.event_mask=StructureNotifyMask|ExposureMask
		|KeyPressMask|PointerMotionMask|ButtonPressMask;
  printf("Visual ID = 0x%.2x\n", (unsigned int)v->visualid);
	if(!(protoatom=XInternAtom(dpy, "WM_PROTOCOLS", False)))
		_throw("Cannot obtain WM_PROTOCOLS atom");
	if(!(deleteatom=XInternAtom(dpy, "WM_DELETE_WINDOW", False)))
		_throw("Cannot obtain WM_DELETE_WINDOW atom");
	if(!(win=XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, width, height, 0,
		v->depth, InputOutput, v->visual,
		CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &swa)))
		_throw("Could not create window");
	XFree(v);  v=NULL;
	XSetWMProtocols(dpy, win, &deleteatom, 1);
	if(!XMapRaised(dpy, win)) _throw("Could not show window");
	XSync(dpy, False);

	if(interactive) dotesti(dpy, I420_PLANAR, "I420 Planar");
	else
	{
		dotest(I420_PLANAR, "I420 Planar");
		printf("\n");
		dotest(YV12_PLANAR, "YV12 Planar");
		printf("\n");
		dotest(YUY2_PACKED, "YUY2 Packed");
		printf("\n");
		dotest(UYVY_PACKED, "UYVY Packed");
		printf("\n");
	}

	bailout:
	if(dpy && win) XDestroyWindow(dpy, win);
	if(dpy) XCloseDisplay(dpy);
	return 0;
}
