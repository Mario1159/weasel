"""
    daslang
    ~~~~~~~~

    Custom Sphinx domain for daScript (daslang) documentation.
    Based on the upstream daScript documentation domain.
"""

from docutils import nodes
from docutils.parsers.rst import directives

from pygments.lexer import RegexLexer, bygroups, words, include
from pygments.token import (
    Comment, Keyword, Name, Number, Operator, Punctuation, String, Text, Token
)

from pygments import lex as _pygments_lex
from pygments.token import STANDARD_TYPES as _PYG_STANDARD_TYPES


def _das_token_class(ttype):
    while ttype is not None:
        if ttype in _PYG_STANDARD_TYPES:
            return _PYG_STANDARD_TYPES[ttype]
        ttype = ttype.parent
    return ''


_DAS_LEXER_SINGLETON = None


def _das_lexer():
    global _DAS_LEXER_SINGLETON
    if _DAS_LEXER_SINGLETON is None:
        _DAS_LEXER_SINGLETON = DaslangLexer()
    return _DAS_LEXER_SINGLETON


def _append_highlighted(signode, text):
    if not text:
        return
    for ttype, value in _pygments_lex(text, _das_lexer()):
        cls = _das_token_class(ttype)
        classes = ['highlight']
        if cls:
            classes.append(cls)
        signode += nodes.inline(value, value, classes=classes)


from sphinx import version_info
from sphinx import addnodes
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, ObjType
if version_info >= (7, 3, 0):
    from sphinx.domains.python._annotations import _pseudo_parse_arglist
else:
    from sphinx.domains.python import _pseudo_parse_arglist
from sphinx.locale import _
from sphinx.roles import XRefRole
from sphinx.util.docfields import Field, GroupedField, TypedField
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode


class DASObject(ObjectDescription):
    has_arguments = False
    skip_empty_arguments = False
    display_prefix = None
    allow_nesting = False

    def handle_signature(self, sig, signode):
        sig = sig.strip()
        sig = sig.split("/*")[0].strip()
        closed_paranteses = sig.rfind(')')
        open_paranteses = -1
        if closed_paranteses >= 0:
            depth = 1
            i = closed_paranteses - 1
            while i >= 0:
                ch = sig[i]
                if ch == ')':
                    depth += 1
                elif ch == '(':
                    depth -= 1
                    if depth == 0:
                        open_paranteses = i
                        break
                i -= 1
        if open_paranteses > 0 and closed_paranteses > open_paranteses:
            member = sig[:open_paranteses].strip()
            arglist = sig[open_paranteses + 1:closed_paranteses].strip()
            retType = sig[closed_paranteses + 1:].strip()
        else:
            member = sig
            arglist = None
            retType = None

        prefix = self.env.ref_context.get('das:object', None)
        mod_name = self.env.ref_context.get('das:module')
        name = member
        try:
            member_prefix, member_name = member.rsplit('.', 1)
        except ValueError:
            member_name = name
            member_prefix = ''
        finally:
            name = member_name
            if prefix and member_prefix:
                prefix = '.'.join([prefix, member_prefix])
            elif prefix is None and member_prefix:
                prefix = member_prefix
        fullname = name
        if prefix:
            fullname = '.'.join([prefix, name])

        signode['module'] = mod_name
        signode['object'] = prefix
        signode['fullname'] = fullname

        if self.display_prefix:
            signode += addnodes.desc_annotation(self.display_prefix,
                                                self.display_prefix)
        if prefix:
            signode += addnodes.desc_addname(prefix + '.', prefix + '.')
        signode += addnodes.desc_name(name, name)
        if self.has_arguments:
            if not arglist:
                if not self.skip_empty_arguments:
                    signode += addnodes.desc_sig_punctuation('(', '(')
                    signode += addnodes.desc_sig_punctuation(')', ')')
            else:
                signode += addnodes.desc_sig_punctuation('(', '(')
                _append_highlighted(signode, arglist)
                signode += addnodes.desc_sig_punctuation(')', ')')
        if retType:
            _append_highlighted(signode, retType)
        return fullname, prefix

    def add_target_and_index(self, name_obj, sig, signode):
        mod_name = self.env.ref_context.get('das:module')
        fullname = (mod_name and mod_name + '.' or '') + name_obj[0]
        id_value = fullname.replace('$', '_S_')
        if id_value not in self.state.document.ids:
            signode['names'].append(fullname)
            signode['ids'].append(id_value)
            signode['first'] = not self.names
            self.state.document.note_explicit_target(signode)
            objects = self.env.domaindata['das']['objects']
            if fullname in objects:
                self.state_machine.reporter.warning(
                    f'duplicate object description of {fullname}, '
                    f'other instance in {self.env.doc2path(objects[fullname][0])}',
                    line=self.lineno)
            objects[fullname] = self.env.docname, self.objtype

        indextext = self.get_index_text(mod_name, name_obj)
        if indextext:
            self.indexnode['entries'].append(('single', indextext,
                                              id_value, '', None))

    def get_index_text(self, objectname, name_obj):
        name, obj = name_obj
        if self.objtype == 'function':
            if not obj:
                return _('%s() (built-in function)') % name
            return _('%s() (%s method)') % (name, obj)
        elif self.objtype == 'class':
            return _('%s() (class)') % name
        elif self.objtype == 'data':
            return _('%s (global variable or constant)') % name
        elif self.objtype == 'attribute':
            return _('%s (%s attribute)') % (name, obj)
        return ''

    def before_content(self):
        prefix = None
        if self.names:
            (obj_name, obj_name_prefix) = self.names.pop()
            prefix = obj_name_prefix.strip('.') if obj_name_prefix else None
            if self.allow_nesting:
                prefix = obj_name
        if prefix:
            self.env.ref_context['das:object'] = prefix
            if self.allow_nesting:
                objects = self.env.ref_context.setdefault('das:objects', [])
                objects.append(prefix)

    def after_content(self):
        objects = self.env.ref_context.setdefault('das:objects', [])
        if self.allow_nesting:
            try:
                objects.pop()
            except IndexError:
                pass
        self.env.ref_context['das:object'] = (objects[-1] if len(objects) > 0
                                             else None)


class DASAttribute(DASObject):
    has_arguments = True
    skip_empty_arguments = True


class DASCallable(DASObject):
    has_arguments = True

    doc_field_types = [
        TypedField('arguments', label=_('Arguments'),
                   names=('argument', 'arg', 'parameter', 'param'),
                   typerolename='func', typenames=('paramtype', 'type')),
        GroupedField('errors', label=_('Throws'), rolename='err',
                     names=('throws', ),
                     can_collapse=True),
        Field('returnvalue', label=_('Returns'), has_arg=False,
              names=('returns', 'return')),
        Field('returntype', label=_('Return type'), has_arg=False,
              names=('rtype',)),
    ]


class DASOperator(DASCallable):
    has_arguments = True
    skip_empty_arguments = True


class DASConstructor(DASCallable):
    display_prefix = 'class '
    allow_nesting = True


class DASModule(SphinxDirective):
    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {
        'noindex': directives.flag
    }

    def run(self):
        mod_name = self.arguments[0].strip()
        self.env.ref_context['das:module'] = mod_name
        noindex = 'noindex' in self.options
        ret = []
        if not noindex:
            self.env.domaindata['das']['modules'][mod_name] = self.env.docname
            self.env.domaindata['das']['objects'][mod_name] = (self.env.docname, 'module')
            targetnode = nodes.target('', '', ids=['module-' + mod_name],
                                      ismod=True)
            self.state.document.note_explicit_target(targetnode)
            ret.append(targetnode)
            indextext = _('%s (module)') % mod_name
            inode = addnodes.index(entries=[('single', indextext,
                                             'module-' + mod_name, '', None)])
            ret.append(inode)
        return ret


class DASXRefRole(XRefRole):
    def process_link(self, env, refnode, has_explicit_title, title, target):
        refnode['das:object'] = env.ref_context.get('das:object')
        refnode['das:module'] = env.ref_context.get('das:module')
        if not has_explicit_title:
            title = title.lstrip('.')
            target = target.lstrip('~')
            if title[0:1] == '~':
                title = title[1:]
                dot = title.rfind('.')
                if dot != -1:
                    title = title[dot + 1:]
        if target[0:1] == '.':
            target = target[1:]
            refnode['refspecific'] = True
        return title, target


class DaslangDomain(Domain):
    name = 'das'
    label = 'Daslang'
    object_types = {
        'function':  ObjType(_('function'),  'func'),
        'method':    ObjType(_('method'),    'meth'),
        'class':     ObjType(_('class'),     'class'),
        'data':      ObjType(_('data'),      'data'),
        'attribute': ObjType(_('attribute'), 'attr'),
        'module':    ObjType(_('module'),    'mod'),
        'operator':  ObjType(_('operator'),  'op'),
    }
    directives = {
        'function':  DASCallable,
        'method':    DASCallable,
        'class':     DASConstructor,
        'data':      DASObject,
        'attribute': DASAttribute,
        'module':    DASModule,
        'operator':  DASOperator,
    }
    roles = {
        'func':  DASXRefRole(fix_parens=True),
        'meth':  DASXRefRole(fix_parens=True),
        'class': DASXRefRole(fix_parens=True),
        'data':  DASXRefRole(),
        'attr':  DASXRefRole(),
        'mod':   DASXRefRole(),
        'op':    DASXRefRole(),
    }
    initial_data = {
        'objects': {},
        'modules': {},
    }

    def clear_doc(self, docname):
        for fullname, (pkg_docname, _l) in list(self.data['objects'].items()):
            if pkg_docname == docname:
                del self.data['objects'][fullname]
        for mod_name, pkg_docname in list(self.data['modules'].items()):
            if pkg_docname == docname:
                del self.data['modules'][mod_name]

    def merge_domaindata(self, docnames, otherdata):
        for fullname, (fn, objtype) in otherdata['objects'].items():
            if fn in docnames:
                self.data['objects'][fullname] = (fn, objtype)
        for mod_name, pkg_docname in otherdata['modules'].items():
            if pkg_docname in docnames:
                self.data['modules'][mod_name] = pkg_docname

    def find_obj(self, env, mod_name, prefix, name, typ, searchorder=0):
        if name[-2:] == '()':
            name = name[:-2]
        objects = self.data['objects']

        searches = []
        if mod_name and prefix:
            searches.append('.'.join([mod_name, prefix, name]))
        if mod_name:
            searches.append('.'.join([mod_name, name]))
        if prefix:
            searches.append('.'.join([prefix, name]))
        searches.append(name)

        if searchorder == 0:
            searches.reverse()

        newname = None
        for search_name in searches:
            if search_name in objects:
                newname = search_name

        return newname, objects.get(newname)

    def resolve_xref(self, env, fromdocname, builder, typ, target, node,
                     contnode):
        mod_name = node.get('das:module')
        prefix = node.get('das:object')
        searchorder = node.hasattr('refspecific') and 1 or 0
        name, obj = self.find_obj(env, mod_name, prefix, target, typ, searchorder)
        if not obj:
            return None
        return make_refnode(builder, fromdocname, obj[0],
                            name.replace('$', '_S_'), contnode, name)

    def resolve_any_xref(self, env, fromdocname, builder, target, node,
                         contnode):
        mod_name = node.get('das:module')
        prefix = node.get('das:object')
        name, obj = self.find_obj(env, mod_name, prefix, target, None, 1)
        if not obj:
            return []
        return [('das:' + self.role_for_objtype(obj[1]),
                 make_refnode(builder, fromdocname, obj[0],
                              name.replace('$', '_S_'), contnode, name))]

    def get_objects(self):
        for refname, (docname, type) in list(self.data['objects'].items()):
            yield refname, refname, type, docname, \
                refname.replace('$', '_S_'), 1

    def get_full_qualified_name(self, node):
        modname = node.get('das:module')
        prefix = node.get('das:object')
        target = node.get('reftarget')
        if target is None:
            return None
        else:
            return '.'.join(filter(None, [modname, prefix, target]))


class DaslangLexer(RegexLexer):
    name = 'daslang'
    aliases = ['das', 'daslang', 'dascript']
    filenames = ['*.das']

    tokens = {
        'root': [
            (r'//.*$', Comment.Single),
            (r'/\*', Comment.Multiline, 'block_comment'),
            (r'"', String.Double, 'string'),
            (r"'[^']*'", String.Char),
            (r'0[xX][0-9a-fA-F_]+[uU]?[lL]?', Number.Hex),
            (r'[0-9][0-9_]*\.[0-9_]*([eE][+-]?[0-9_]+)?[fFlL]?', Number.Float),
            (r'[0-9][0-9_]*[uU]?[lL]?', Number.Integer),
            (words((
                'struct', 'class', 'let', 'var', 'def', 'while', 'if', 'static_if',
                'else', 'elif', 'static_elif', 'for', 'finally', 'in', 'is', 'as',
                'where', 'return', 'yield', 'break', 'continue',
                'pass', 'try', 'recover', 'delete', 'deref',
                'new', 'typeinfo', 'type', 'typedecl', 'array', 'table',
                'block', 'function', 'lambda', 'generator',
                'expect', 'override', 'abstract', 'sealed',
                'require', 'module', 'public', 'private',
                'options', 'operator', 'enum', 'typedef', 'variant', 'tuple',
                'with', 'cast', 'upcast', 'reinterpret', 'aka',
                'assume', 'unsafe', 'addr', 'label', 'goto',
                'implicit', 'explicit', 'shared', 'smart_ptr', 'inscope',
                'static', 'fixed_array', 'iterator', 'bitfield',
                'capture', 'template', 'const', 'default', 'uninitialized',
            ), prefix=r'\b', suffix=r'\b'), Keyword),
            (words(('true', 'false', 'null'), prefix=r'\b', suffix=r'\b'), Keyword.Constant),
            (words((
                'void', 'bool', 'string', 'auto',
                'int', 'int2', 'int3', 'int4', 'int8', 'int16', 'int64',
                'uint', 'uint2', 'uint3', 'uint4', 'uint8', 'uint16', 'uint64',
                'float', 'float2', 'float3', 'float4',
                'double', 'range', 'urange', 'range64', 'urange64',
            ), prefix=r'\b', suffix=r'\b'), Keyword.Type),
            (r'\[[\w]+\]', Name.Decorator),
            (r'@@?', Operator),
            (r'\$', Operator),
            (r'[a-zA-Z_]\w*', Name),
            (r'[+\-*/%&|^~<>=!?:#]+', Operator),
            (r'[{}()\[\];,.]', Punctuation),
            (r'\|>', Operator),
            (r'<-', Operator),
            (r'->', Operator),
            (r'<\|', Operator),
            (r'=>', Operator),
            (r'\s+', Text),
            (r'.', Text),
        ],
        'block_comment': [
            (r'/\*', Comment.Multiline, '#push'),
            (r'\*/', Comment.Multiline, '#pop'),
            (r'[^/*]+', Comment.Multiline),
            (r'[/*]', Comment.Multiline),
        ],
        'string': [
            (r'\\[\\nrt"\'{}]', String.Escape),
            (r'\{', String.Interpol, 'interpolation'),
            (r'[^"\\{]+', String.Double),
            (r'"', String.Double, '#pop'),
        ],
        'interpolation': [
            (r'\}', String.Interpol, '#pop'),
            include('root'),
        ],
    }


def setup(app):
    app.add_domain(DaslangDomain)
    app.add_lexer('das', DaslangLexer)

    return {
        'version': 'builtin',
        'env_version': 1,
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
