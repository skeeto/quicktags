// A UTF-8 BOM before the first line. Public domain.
class Bom {
public:
    int marker() const;
};

int Bom::marker() const { return 0xfeff; }

Bom first_bom;
