#ifndef _INIT_H_
#define _INIT_H_

/* Initialize the atom variables and atom arrays */
void init_atoms(void);

/* Allocate colors in the config structure */
void init_colors(void);

/* Initialize cursors */
void init_cursor(void);

/* create dummy windows used for controlling the layer of clients */
void init_layerwindows(void);

/* Get X resources and update variables in the config structure */
void init_resources(void);

#endif /* _INIT_H_ */
