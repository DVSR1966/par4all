ifdef USE_DEPS
fwd-contrainte:fwd-vecteur
fwd-sc:fwd-contrainte
fwd-sc:fwd-vecteur
fwd-ray_dte:fwd-contrainte
fwd-ray_dte:fwd-vecteur
fwd-sommet:fwd-contrainte
fwd-sommet:fwd-ray_dte
fwd-sommet:fwd-sc
fwd-sommet:fwd-vecteur
fwd-sparse_sc:fwd-contrainte
fwd-sparse_sc:fwd-matrice
fwd-sparse_sc:fwd-matrix
fwd-sparse_sc:fwd-sc
fwd-sparse_sc:fwd-vecteur
fwd-sg:fwd-ray_dte
fwd-sg:fwd-sommet
fwd-sg:fwd-vecteur
fwd-polynome:fwd-vecteur
fwd-union:fwd-contrainte
fwd-union:fwd-polyedre
fwd-union:fwd-sc
fwd-union:fwd-sommet
fwd-union:fwd-vecteur
fwd-polyedre:fwd-contrainte
fwd-polyedre:fwd-polynome
fwd-polyedre:fwd-ray_dte
fwd-polyedre:fwd-sc
fwd-polyedre:fwd-sg
fwd-polyedre:fwd-sommet
fwd-polyedre:fwd-vecteur
endif # USE_DEPS