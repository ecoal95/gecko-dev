use std::iter::IntoIterator;

use syntax::ast;
use syntax::attr;
use syntax::codemap::{DUMMY_SP, Span, respan};
use syntax::parse::token;
use syntax::ptr::P;

use invoke::{Invoke, Identity};
use lit::LitBuilder;
use str::ToInternedString;

//////////////////////////////////////////////////////////////////////////////

pub struct AttrBuilder<F=Identity> {
    callback: F,
    span: Span,
    style: ast::AttrStyle,
    is_sugared_doc: bool,
}

impl AttrBuilder {
    pub fn new() -> Self {
        AttrBuilder::with_callback(Identity)
    }
}

impl<F> AttrBuilder<F>
    where F: Invoke<ast::Attribute>,
{
    pub fn with_callback(callback: F) -> Self {
        AttrBuilder {
            callback: callback,
            span: DUMMY_SP,
            style: ast::AttrStyle::Outer,
            is_sugared_doc: false,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn inner(mut self) -> Self {
        self.style = ast::AttrStyle::Inner;
        self
    }

    pub fn build_meta_item(self, item: P<ast::MetaItem>) -> F::Result {
        let attr = respan(self.span, ast::Attribute_ {
            id: attr::mk_attr_id(),
            style: self.style,
            value: item,
            is_sugared_doc: self.is_sugared_doc,
        });
        self.callback.invoke(attr)
    }

    pub fn build_meta_item_(self, item: ast::MetaItemKind) -> F::Result {
        let item = P(respan(self.span, item));
        self.build_meta_item(item)
    }

    pub fn word<T>(self, word: T) -> F::Result
        where T: ToInternedString
    {
        self.build_meta_item_(ast::MetaItemKind::Word(word.to_interned_string()))
    }

    pub fn list<T>(self, word: T) -> AttrListBuilder<Self>
        where T: ToInternedString
    {
        AttrListBuilder::with_callback(word, self)
    }

    pub fn name_value<T>(self, name: T) -> LitBuilder<AttrNameValueBuilder<Self>>
        where T: ToInternedString,
    {
        LitBuilder::with_callback(AttrNameValueBuilder {
            callback: self,
            name: name.to_interned_string(),
        })
    }

    pub fn automatically_derived(self) -> F::Result {
        self.word("automatically_derived")
    }

    pub fn inline(self) -> F::Result {
        self.word("inline")
    }

    pub fn test(self) -> F::Result {
        self.word("test")
    }

    pub fn allow<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        self.list("allow").words(iter).build()
    }

    pub fn warn<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        self.list("warn").words(iter).build()
    }

    pub fn deny<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        self.list("deny").words(iter).build()
    }

    pub fn features<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        self.list("feature").words(iter).build()
    }

    pub fn plugins<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        self.list("plugin").words(iter).build()
    }

    /**
     * Create a #[doc = "..."] node. Note that callers of this must make sure to prefix their
     * comments with either "///" or "/\*\*" if an outer comment, or "//!" or "/\*!" if an inner
     * comment.
     */
    pub fn doc<T>(mut self, doc: T) -> F::Result
        where T: ToInternedString,
    {
        self.is_sugared_doc = true;
        self.name_value("doc").str(doc)
    }
}

impl<F> Invoke<P<ast::MetaItem>> for AttrBuilder<F>
    where F: Invoke<ast::Attribute>,
{
    type Result = F::Result;

    fn invoke(self, item: P<ast::MetaItem>) -> F::Result {
        self.build_meta_item(item)
    }
}

impl<F> Invoke<ast::MetaItemKind> for AttrBuilder<F>
    where F: Invoke<ast::Attribute>,
{
    type Result = F::Result;

    fn invoke(self, item: ast::MetaItemKind) -> F::Result {
        self.build_meta_item_(item)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct AttrListBuilder<F> {
    callback: F,
    span: Span,
    name: token::InternedString,
    items: Vec<ast::NestedMetaItem>,
}

impl<F> AttrListBuilder<F>
    where F: Invoke<P<ast::MetaItem>>,
{
    pub fn with_callback<T>(name: T, callback: F) -> Self
        where T: ToInternedString,
    {
        AttrListBuilder {
            callback: callback,
            span: DUMMY_SP,
            name: name.to_interned_string(),
            items: vec![],
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_meta_items<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::MetaItem>>,
    {
        let span = self.span;
        self.items.extend(iter.into_iter().map(|meta_item| {
            respan(span, ast::NestedMetaItemKind::MetaItem(meta_item))
        }));
        self
    }

    pub fn with_meta_items_<I>(self, iter: I) -> Self
        where I: IntoIterator<Item=ast::MetaItemKind>,
    {
        let iter = iter.into_iter();
        let span = self.span;
        self.with_meta_items(iter.map(|item| P(respan(span, item))))
    }

    pub fn with_meta_item(mut self, item: P<ast::MetaItem>) -> Self {
        self.items.push(respan(self.span, ast::NestedMetaItemKind::MetaItem(item)));
        self
    }

    pub fn with_meta_item_kind(self, item: ast::MetaItemKind) -> Self {
        let span = self.span;
        self.with_meta_item(P(respan(span, item)))
    }

    pub fn words<I, T>(self, iter: I) -> Self
        where I: IntoIterator<Item=T>,
              T: ToInternedString,
    {
        let iter = iter.into_iter();
        self.with_meta_items_(iter.map(|word| ast::MetaItemKind::Word(word.to_interned_string())))
    }

    pub fn word<T>(self, word: T) -> Self
        where T: ToInternedString,
    {
        self.with_meta_item_kind(ast::MetaItemKind::Word(word.to_interned_string()))
    }

    pub fn list<T>(self, name: T) -> AttrListBuilder<Self>
        where T: ToInternedString,
    {
        AttrListBuilder::with_callback(name, self)
    }

    pub fn name_value<T>(self, name: T) -> LitBuilder<AttrNameValueBuilder<Self>>
        where T: ToInternedString,
    {
        LitBuilder::with_callback(AttrNameValueBuilder {
            callback: self,
            name: name.to_interned_string(),
        })
    }

    pub fn build(self) -> F::Result {
        let item = respan(self.span, ast::MetaItemKind::List(self.name, self.items));
        self.callback.invoke(P(item))
    }
}

impl<F> Invoke<P<ast::MetaItem>> for AttrListBuilder<F>
    where F: Invoke<P<ast::MetaItem>>,
{
    type Result = Self;

    fn invoke(self, item: P<ast::MetaItem>) -> Self {
        self.with_meta_item(item)
    }
}

impl<F> Invoke<ast::MetaItemKind> for AttrListBuilder<F>
    where F: Invoke<P<ast::MetaItem>>,
{
    type Result = Self;

    fn invoke(self, item: ast::MetaItemKind) -> Self {
        self.with_meta_item_kind(item)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct AttrNameValueBuilder<F> {
    callback: F,
    name: token::InternedString,
}

impl<F: Invoke<ast::MetaItemKind>> Invoke<P<ast::Lit>> for AttrNameValueBuilder<F> {
    type Result = F::Result;

    fn invoke(self, value: P<ast::Lit>) -> F::Result {
        let item = ast::MetaItemKind::NameValue(self.name, (*value).clone());
        self.callback.invoke(item)
    }
}
