#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

extern SEXP rayplot_open_(SEXP, SEXP, SEXP, SEXP, SEXP);
extern SEXP rayplot_step_(SEXP);
extern SEXP rayplot_should_close_(SEXP);
extern SEXP rayplot_close_(SEXP);

extern SEXP rayplot3D_open_(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
extern SEXP rayplot3D_step_(SEXP);
extern SEXP rayplot3D_close_(SEXP);

static const R_CallMethodDef CallEntries[] = {
    {"rayplot_open_",         (DL_FUNC) &rayplot_open_,         5},
    {"rayplot_step_",         (DL_FUNC) &rayplot_step_,         1},
    {"rayplot_should_close_", (DL_FUNC) &rayplot_should_close_, 1},
    {"rayplot_close_",        (DL_FUNC) &rayplot_close_,        1},
    {"rayplot3D_open_",       (DL_FUNC) &rayplot3D_open_,       7},
    {"rayplot3D_step_",       (DL_FUNC) &rayplot3D_step_,       1},
    {"rayplot3D_close_",      (DL_FUNC) &rayplot3D_close_,      1},
    {NULL, NULL, 0}
};

void R_init_rayplot(DllInfo *dll) { // Make sure this matches your exact package name setup
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
