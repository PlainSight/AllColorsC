#include<stdio.h>
#include<stdlib.h>
#include<time.h>

#include "allcolors.h"
#include "lodepng.h"

#define ARRAYSIZE 48
#define ARRAYSHRINK 36

#define REDBITDEPTH 8
#define GREENBITDEPTH 8
#define BLUEBITDEPTH 8

#define WIDTH 4096
#define HEIGHT 4096
#define TODO 16777216 //(WIDTH*HEIGHT)
#define COLORS 16777216 //256*256*256

struct SuperColor {
	octtree* location;
	int x;
	int y;
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

struct OctTree {
	supercolor* colors[ARRAYSIZE];
	octtree* children[8];
	octtree* parent;
	int minx;
	int miny;
	int minz;
	int maxx;
	int maxy;
	int maxz;
	int size;
	//hasChildren, 0 = no and not initialized, 1 = no but initialized, 2 = yes
	unsigned short hasChildren;
};

supercolor createSuperColor(int r, int g, int b) {

	supercolor color;
	color.r = (unsigned char) r;
	color.g = (unsigned char) g;
	color.b = (unsigned char) b;

	return color;
}

octtree* createOctTree(int minx, int miny, int minz,
	int maxx, int maxy, int maxz,
	octtree* p, octtree* tree) {

	tree->parent = p;

	tree->minx = minx;
	tree->maxx = maxx;

	tree->miny = miny;
	tree->maxy = maxy;

	tree->minz = minz;
	tree->maxz = maxz;

	//tree->hasChildren = 0;
	//tree->size = 0;

	return tree;
}

int getColorDistance(supercolor* color, supercolor* other) {

	int rd = (int) color->r - (int) other->r;
	int gd = (int) color->g - (int) other->g;
	int bd = (int) color->b - (int) other->b;

	return rd*rd + gd*gd + bd*bd;
}

void putColorInChildTree(octtree* tree, supercolor* color) {
	int index = 4 * (2 * color->r > tree->maxx + tree->minx)
						+	2 * (2 * color->g > tree->maxy + tree->miny)
						+			(2 * color->b > tree->maxz + tree->minz);

	putColorInTree(tree->children[index], color);
}

void removeFromTree(octtree* tree, supercolor* color) {

	for(int i = 0; i < ARRAYSIZE; i++) {
		if(tree->colors[i] == color) {
			tree->colors[i] = tree->colors[tree->size - 1];
			break;
		}
	}

	for(octtree* t = tree; t != NULL; t = t->parent) {
		(t->size)--;

		if(t->size < ARRAYSHRINK && t->hasChildren == 2) {

			int tColorIndex = 0;

			//foreach child get all the colors
			for(int i = 0; i < 8; i++) {
				for (int j = 0; j < t->children[i]->size; j++) {
					t->colors[tColorIndex++] = t->children[i]->colors[j];
				}
				t->children[i]->hasChildren = t->children[i]->hasChildren == 2 ? 1 : 0;
				t->children[i]->size = 0;
			}

			for(int i = 0; i < t->size; i++) {
				t->colors[i]->location = t;
			}

			t->hasChildren = 1;
		}
	}
}

void putColorInTree(octtree* tree, supercolor* color) {

	if(tree->hasChildren == 2) {
		putColorInChildTree(tree, color);
	} else {
		if(tree->size < ARRAYSIZE) {
			tree->colors[tree->size] = color;
			color->location = tree;
		} else {
			splitOctTree(tree);
			putColorInChildTree(tree, color);
		}
	}

	(tree->size)++;
}

int shouldVisitTree(octtree* tree, supercolor* nom, supercolor* nearest) {

	//mid values

	int mx = (tree->minx + tree->maxx) / 2;
	int my = (tree->miny + tree->maxy) / 2;
	int mz = (tree->minz + tree->maxz) / 2;

	//closest corner values

	int cx = (nom->r < mx) * tree->minx + (nom->r > mx) * tree->maxx;
	int cy = (nom->g < my) * tree->miny + (nom->g > my) * tree->maxy;
	int cz = (nom->b < mz) * tree->minz + (nom->b > mz) * tree->maxz;

	int isoutx = (nom->r >= tree->maxx) | (nom->r < tree->minx);
	int isouty = (nom->g >= tree->maxy) | (nom->g < tree->miny);
	int isoutz = (nom->b >= tree->maxz) | (nom->b < tree->minz);

	int dx = isoutx * (((int)nom->r) - cx);
	int dy = isouty * (((int)nom->g) - cy);
	int dz = isoutz * (((int)nom->b) - cz);

	int distancesqr = dx*dx + dy*dy + dz*dz;

	return getColorDistance(nom, nearest) > distancesqr;
}

supercolor* findNearestColorInTree(octtree* tree, supercolor* nom, supercolor* nearest) {

	if(tree->size == 0 || (nearest != NULL && !shouldVisitTree(tree, nom, nearest))) {
		return nearest;
	}

	if(tree->hasChildren < 2) {
		for(int i = 0; i < tree->size; i++) {
			if(nearest == NULL) {
				nearest = tree->colors[i];
			} else {
				if(getColorDistance(nom, tree->colors[i]) < getColorDistance(nom, nearest)) {
					nearest = tree->colors[i];
				}
			}
		}
		return nearest;
	} else {
		//get a good first nearest candidate

		int bestChild = 4 * (2 * nom->r > tree->maxx + tree->minx)
									+	2 * (2 * nom->g > tree->maxy + tree->miny)
									+			(2 * nom->b > tree->maxz + tree->minz);

		nearest = findNearestColorInTree(tree->children[bestChild], nom, nearest);

		for(int i = 0; i < 8; i++) {
			if (i == bestChild) continue;

			nearest = findNearestColorInTree(tree->children[i], nom, nearest);
		}
		return nearest;
	}
}

void splitOctTree(octtree* tree) {

	int midx = ( tree->minx + tree->maxx ) / 2;
	int midy = ( tree->miny + tree->maxy ) / 2;
	int midz = ( tree->minz + tree->maxz ) / 2;

	if(tree->hasChildren == 0) {
		octtree* memory = (octtree *) calloc(8, sizeof(octtree));

		tree->children[0] = createOctTree(tree->minx, tree->miny, tree->minz,
										  		midx, 		midy, 		midz, 
										  tree, &memory[0]);
		tree->children[1] = createOctTree(tree->minx, tree->miny, 		midz,
										  		midx, 		midy, tree->maxz, 
										  tree, &memory[1]);
		tree->children[2] = createOctTree(tree->minx, 		midy, tree->minz,
										  		midx, tree->maxy, 		midz, 
										  tree, &memory[2]);
		tree->children[3] = createOctTree(tree->minx, 		midy, 		midz,
										  		midx, tree->maxy, tree->maxz, 
										  tree, &memory[3]);

		tree->children[4] = createOctTree(		midx, tree->miny, tree->minz,
										  tree->maxx, 		midy, 		midz, 
										  tree, &memory[4]);
		tree->children[5] = createOctTree(		midx, tree->miny, 		midz,
										  tree->maxx, 		midy, tree->maxz, 
										  tree, &memory[5]);
		tree->children[6] = createOctTree(		midx, 		midy, tree->minz,
										  tree->maxx, tree->maxy, 		midz, 
										  tree, &memory[6]);
		tree->children[7] = createOctTree(		midx, 		midy, 		midz,
										  tree->maxx, tree->maxy, tree->maxz, 
										  tree, &memory[7]);
	}

	tree->hasChildren = 2;

	for (int i = 0; i < ARRAYSIZE; i++) {
		putColorInChildTree(tree, tree->colors[i]);
	}
}

void outImage(const char* filename, const unsigned char* image, unsigned width, unsigned height)
{
  /*Encode the image*/
  unsigned error = lodepng_encode32_file(filename, image, width, height);

  /*if there's an error, display it*/
  if(error) printf("error %u: %s\n", error, lodepng_error_text(error));
}

void setPixel(unsigned int* open, supercolor* color, octtree* tree, int* pseudoRandom) {
	int set = 0;

	int openSpaces[8][2];

	while(!set) {
		
		supercolor* closestNeighbour = findNearestColorInTree(tree, color, NULL);
						
		int minx = closestNeighbour->x - 1;
		int maxx = closestNeighbour->x + 1;
		int miny = closestNeighbour->y - 1;
		int maxy = closestNeighbour->y + 1;

		minx = minx < 0 ? 0 : minx;
		maxx = maxx >= WIDTH ? WIDTH-1 : maxx;
		miny = miny < 0 ? 0 : miny;
		maxy = maxy >= HEIGHT ? HEIGHT-1 : maxy;
		
		int numopen = 0;

		for(int x = minx; x <= maxx; x++) {
			for(int y = miny; y <= maxy; y++) {

				int place = (y * HEIGHT) + x;
				int bindex = place / (8*sizeof(int));
				int b = place % (8*sizeof(int));

				if(!(open[bindex] & (1 << b))) {
					openSpaces[numopen][0] = x;
					openSpaces[numopen][1] = y;
					numopen++;
					set = 1;
				}
			}
		}
		
		if(!set) {
			removeFromTree(closestNeighbour->location, closestNeighbour);			
		} else {
			//if there is only 1 space left the closest color will no longer be a valid neighbour after this color is placed
			if(numopen == 1) {
				removeFromTree(closestNeighbour->location, closestNeighbour);
			}

			int placement = (*pseudoRandom) % numopen;

			int placeX = openSpaces[placement][0];
			int placeY = openSpaces[placement][1];
			
			(*pseudoRandom)++;

			int place = (placeY * HEIGHT) + placeX;
			int bindex = place / (8*sizeof(int));
			int b = place % (8*sizeof(int));

			open[bindex] |= (1 << b);

			color->x = placeX;
			color->y = placeY;

			// avoid putting the color in the tree if at all possible
			/*if(numopen == 1) {
				
				minx = placeX - 1;
				maxx = placeX + 1;
				miny = placeY - 1;
				maxy = placeY + 1;

				minx = minx < 0 ? 0 : minx;
				maxx = maxx >= WIDTH ? WIDTH-1 : maxx;
				miny = miny < 0 ? 0 : miny;
				maxy = maxy >= HEIGHT ? HEIGHT-1 : maxy;
				
				int openNeighbours = 0;

				for(int x = minx; x <= maxx; x++) {
					for(int y = miny; y <= maxy; y++) {

					 	place = (y * HEIGHT) + x;
						bindex = place / (8*sizeof(int));
						b = place % (8*sizeof(int));

						if(!(open[bindex] & (1 << b))) {
							openNeighbours++;
						}
					}
				}

				if(openNeighbours == 0) {
					return;
				}
			}*/

			putColorInTree(tree, color);
		}
	}
}

void generateImage(supercolor* colors, char* filename) {
	unsigned char* image = (unsigned char*) calloc(WIDTH * HEIGHT, 4);

	for(int i = 0; i < WIDTH*HEIGHT; i++) {
		supercolor col = colors[i];

		int place = 4 * ((col.y * WIDTH) + col.x);

		image[place] = col.r;
		image[place+1] = col.g;
		image[place+2] = col.b;
		image[place+3] = 255;
	}

	outImage(filename, image, WIDTH, HEIGHT);
}

int main() {

	setbuf(stdout, NULL);

	octtree* root = createOctTree(0, 0, 0, 256, 256, 256, NULL, (octtree *) calloc(1, sizeof(octtree)));

	supercolor* colors = (supercolor *) calloc(COLORS, sizeof(supercolor));

	int redMask = 0x100 - (1 << (8 - REDBITDEPTH));
	int greenMask = 0x100 - (1 << (8 - GREENBITDEPTH));
	int blueMask = 0x100 - (1 << (8 - BLUEBITDEPTH));

	printf("masks:\nred: %x\ngreen: %x\nblue: %x\n", redMask, greenMask, blueMask);

	redMask = redMask << 16;
	greenMask = greenMask << 8;

	for(int i = 0; i < COLORS; i++) {
		int r = (i & redMask) >> 16;
		int g = (i & greenMask) >> 8;
		int b = i & blueMask;
		colors[i] = createSuperColor(r, g, b);
	}

	srand(time(NULL));

	for(int i = 0; i < COLORS; i++) {
		int r = ((rand() & 0xFF) << 16) | (rand() & 0x0000FFFF);

		int random = i + (r % (COLORS - i));

		supercolor temp = colors[i];
		colors[i] = colors[random];
		colors[random] = temp;
	}

	printf("created colors\n");

	unsigned int* open = (unsigned int*) calloc(WIDTH*HEIGHT/(8*sizeof(int)), sizeof(int));

	int pseudoRandom = 0;

	time_t start = time(0);
	
	colors[0].x = WIDTH / 2;
	colors[0].y = HEIGHT / 2;

	int place = ((HEIGHT * HEIGHT / 2) + WIDTH / 2);
	int bindex = place / (8*sizeof(int));
	int b = place % (8*sizeof(int));

	open[bindex] |= (1 << b);

	putColorInTree(root, &colors[0]);

	for(int i = 1; i < TODO; i++) {
		setPixel(open, &colors[i], root, &pseudoRandom);

		if (i % 200000 == 0) {
			time_t diff = time(0) - start;

			int s = diff;

			printf("\r%d%% complete, time taken %ds", (int)((i * 100) / (float)TODO), s);

			//printf("set %d, %d \n", colors[i].x, colors[i].y);
			//char fn[50];
			//sprintf(fn, "%d completed.png", i);
			//generateImage(colors, fn);
		}
	}

	time_t diff = time(0) - start;

	time_t now = time(0);
	char filename[50];

	printf("\n");

	sprintf(filename, "picture %li - %lis.png", now, diff);

	generateImage(colors, filename);
}
