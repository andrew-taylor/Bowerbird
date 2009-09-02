
/* tonal_model/generate.c */

void model_to_sound_file(tonal_model_t *t, double sampling_rate, char *wav_file);
double *generate_sound(tonal_model_t *t, double sampling_rate, index_t *n_samples);
void generate_modulation(tonal_model_t *t, int which, index_t n_samples, double *samples);
void apply_modifiers(modifier_t *m, int which, index_t n_samples, double *samples);
void calculate_contour(contour_t *c, index_t n_contour, double *contour);
double contour_length(contour_t *c, int recurse);
double chorus_length(tonal_model_t *t, int recurse);
double variable_value_or_default(model_variable_t *v, char *name, double default_value);
double variable_value(model_variable_t *v, char *name);
model_variable_t *find_variable(model_variable_t *v, char *name);
index_t max_u(index_t x, index_t y) __attribute__ ((const)) ;
index_t min_u(index_t x, index_t y) __attribute__ ((const)) ;

/* tonal_model/read_model.c */

tonal_model_t *read_model(char *filename);
tonal_model_t *read_model_txt(char *filename);
tonal_model_t *read_model_xml(char *filename);
tonal_model_t *read_model_xml_stream(FILE *f, char *base_url);
tonal_model_t *read_model_xml_fd(int fd, char *base_url);
#ifdef OLD
tonal_model_t *xml_parse_file(char *filename);
#endif
void xml_print(FILE *stream, xmlNodePtr cur, int depth);
tonal_model_t *xml_to_chorus(xmlNodePtr p);
tonal_model_t *xml_to_tonal_model(xmlNodePtr p, int recurse);
modifier_t *xml_to_modifiers(xmlNodePtr p);
modifier_t *xml_to_harmonics(xmlNodePtr p);
contour_t *xml_to_contour(xmlNodePtr p);
contour_t *xml_to_curve(xmlNodePtr p);
model_variable_t *xml_to_parameters(xmlNodePtr p, int recurse);
char *xml_sstrdup_attribute(xmlNodePtr p, char *attribute, char *default_value);
double xml_get_numerical_attribute(xmlNodePtr p, char *attribute, double default_value);
char *xml_get_attribute(xmlNodePtr p, char *attribute);
xmlNodePtr xml_expect(xmlNodePtr p, char *expected);
xmlNodePtr xml_ignore_non_elements(xmlNodePtr p);
int xml_name_strcmp(xmlNodePtr p, char *n);

/* tonal_model/generate_test.c */


/* tonal_model/model_to_sound.c */

