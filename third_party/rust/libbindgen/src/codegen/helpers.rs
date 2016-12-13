//! Helpers for code generation that don't need macro expansion.

use aster;
use ir::layout::Layout;
use syntax::ast;
use syntax::ptr::P;


pub mod attributes {
    use aster;
    use syntax::ast;

    pub fn repr(which: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("repr").words(&[which]).build()
    }

    pub fn repr_list(which_ones: &[&str]) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("repr").words(which_ones).build()
    }

    pub fn derives(which_ones: &[&str]) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("derive").words(which_ones).build()
    }

    pub fn inline() -> ast::Attribute {
        aster::AstBuilder::new().attr().word("inline")
    }

    pub fn doc(comment: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().doc(comment)
    }

    pub fn link_name(name: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().name_value("link_name").str(name)
    }
}

/// Generates a proper type for a field or type with a given `Layout`, that is,
/// a type with the correct size and alignment restrictions.
pub struct BlobTyBuilder {
    layout: Layout,
}

impl BlobTyBuilder {
    pub fn new(layout: Layout) -> Self {
        BlobTyBuilder {
            layout: layout,
        }
    }

    pub fn build(self) -> P<ast::Ty> {
        use std::cmp;

        let ty_name = match self.layout.align {
            8 => "u64",
            4 => "u32",
            2 => "u16",
            1 | _ => "u8",
        };
        let data_len = if ty_name == "u8" {
            self.layout.size
        } else {
            self.layout.size / cmp::max(self.layout.align, 1)
        };

        let inner_ty = aster::AstBuilder::new().ty().path().id(ty_name).build();
        if data_len == 1 {
            inner_ty
        } else {
            aster::ty::TyBuilder::new().array(data_len).build(inner_ty)
        }
    }
}

pub mod ast_ty {
    use aster;
    use ir::context::BindgenContext;
    use ir::ty::FloatKind;
    use syntax::ast;
    use syntax::ptr::P;

    pub fn raw_type(ctx: &BindgenContext, name: &str) -> P<ast::Ty> {
        let ident = ctx.rust_ident_raw(&name);
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = ctx.rust_ident_raw(prefix);
                quote_ty!(ctx.ext_cx(), $prefix::$ident)
            }
            None => quote_ty!(ctx.ext_cx(), ::std::os::raw::$ident),
        }
    }

    pub fn float_kind_rust_type(ctx: &BindgenContext,
                                fk: FloatKind)
                                -> P<ast::Ty> {
        // TODO: we probably should just take the type layout into
        // account?
        //
        // Also, maybe this one shouldn't be the default?
        //
        // FIXME: `c_longdouble` doesn't seem to be defined in some
        // systems, so we use `c_double` directly.
        match (fk, ctx.options().convert_floats) {
            (FloatKind::Float, true) => aster::ty::TyBuilder::new().f32(),
            (FloatKind::Double, true) |
            (FloatKind::LongDouble, true) => aster::ty::TyBuilder::new().f64(),
            (FloatKind::Float, false) => raw_type(ctx, "c_float"),
            (FloatKind::Double, false) |
            (FloatKind::LongDouble, false) => raw_type(ctx, "c_double"),
            (FloatKind::Float128, _) => {
                aster::ty::TyBuilder::new().array(16).u8()
            }
        }
    }

    pub fn int_expr(val: i64) -> P<ast::Expr> {
        use std::i64;
        let expr = aster::AstBuilder::new().expr();

        // This is not representable as an i64 if it's negative, so we
        // special-case it.
        //
        // Fix in aster incoming.
        if val == i64::MIN {
            expr.neg().uint(1u64 << 63)
        } else {
            expr.int(val)
        }
    }

    pub fn bool_expr(val: bool) -> P<ast::Expr> {
        aster::AstBuilder::new().expr().bool(val)
    }

    pub fn byte_array_expr(bytes: &[u8]) -> P<ast::Expr> {
        let mut vec = Vec::with_capacity(bytes.len() + 1);
        for byte in bytes {
            vec.push(int_expr(*byte as i64));
        }
        vec.push(int_expr(0));

        let kind = ast::ExprKind::Vec(vec);

        aster::AstBuilder::new().expr().build_expr_kind(kind)
    }

    pub fn cstr_expr(mut string: String) -> P<ast::Expr> {
        string.push('\0');
        aster::AstBuilder::new()
            .expr()
            .build_lit(aster::AstBuilder::new().lit().byte_str(string))
    }

    pub fn float_expr(f: f64) -> P<ast::Expr> {
        use aster::str::ToInternedString;
        let mut string = f.to_string();

        // So it gets properly recognised as a floating point constant.
        if !string.contains('.') {
            string.push('.');
        }

        let interned_str = string.as_str().to_interned_string();
        let kind = ast::LitKind::FloatUnsuffixed(interned_str);
        aster::AstBuilder::new().expr().lit().build_lit(kind)
    }
}
