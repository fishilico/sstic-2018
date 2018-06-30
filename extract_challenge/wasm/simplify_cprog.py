#!/usr/bin/env python3
"""Simplify C code from wasm2c

./simplify_cprog.py |tee blockcipher.wasm2c.simplified.c && grep -A40 ' _decrypt' blockcipher.wasm2c.simplified.c || ./simplify_cprog.py
"""
import collections
import re

class CurrentVariables:
    def __init__(self, indent):
        self.vars = collections.OrderedDict()
        self.indent = indent
        self.dependent_variables = set()

    def dump(self):
        is_verbose = (len(self.vars) > 0) and 0
        if is_verbose:
            print("/* start dump */")
        for k, v in self.vars.items():
            print('%s%s = %s;' % (self.indent, k, v))
        self.vars = collections.OrderedDict()
        self.dependent_variables = set()
        if is_verbose:
            print("/* end dump */")

    def get_with_paren_if_space(self, name):
        v = self.vars[name]
        if ' ' in v:
            return '(%s)' % v
        return v

    def assign(self, varname, expr):
        """Show an assign if it is not supported, otherwise add the expr or simplify it"""
        if varname in self.dependent_variables:
            # Update to a variable which is used by others => dump!
            self.dump()

        # constant
        m = re.match('([0-9]+)u(ll)?$', expr)
        if m is not None:
            if varname in self.vars:
                self.vars.pop(varname) # drop previous assignment, keeping order
            self.vars[varname] = hex(int(m.group(1)))
            return

        # Other variable
        m = re.match('([a-z][0-9]+)$', expr)
        if m is not None:
            if expr in self.vars:
                expr = self.vars[expr]
            else:
                self.dependent_variables.add(expr)
            self.vars[varname] = expr
            return

        # Load memory from variable
        m = re.match(r'i64_load\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+)\)\)$', expr)
        if m is not None:
            memvar = m.group(1)
            if memvar in self.vars:
                memvar = self.vars[memvar]
            else:
                self.dependent_variables.add(memvar)
            self.vars[varname] = 'MEM_as_u64[%s]' % memvar
            return
        m = re.match(r'i32_load8_u\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+)\)\)$', expr)
        if m is not None:
            memvar = m.group(1)
            if memvar in self.vars:
                memvar = self.vars[memvar]
            else:
                self.dependent_variables.add(memvar)
            self.vars[varname] = '(u32)MEM_as_u8[%s]' % memvar
            return
        m = re.match(r'i32_load8_s\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+)\)\)$', expr)
        if m is not None:
            memvar = m.group(1)
            if memvar in self.vars:
                memvar = self.vars[memvar]
            else:
                self.dependent_variables.add(memvar)
            self.vars[varname] = '(s32)MEM_as_s8[%s]' % memvar
            return

        # Load memory from variable + offset
        m = re.match(r'i64_load\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+) \+ ([0-9]+)\)\)$', expr)
        if m is not None:
            memvar, offset_s = m.groups()
            if memvar in self.vars:
                memvar = self.get_with_paren_if_space(memvar)
            else:
                self.dependent_variables.add(memvar)
            self.vars[varname] = 'MEM_as_u64[%s + %#x]' % (memvar, int(offset_s))
            return

        # default: reset the content
        self.dump()
        self.vars[varname] = expr
        self.dump()

    def update_with_biop(self, varname, op, other):
        if varname in self.dependent_variables:
            # Update to a variable which is used by others => dump!
            self.dump()

        if other in self.vars:
            other = self.get_with_paren_if_space(other)
        else:
            self.dependent_variables.add(other)

        if varname in self.vars:
            expr = self.vars.pop(varname)
            if ' ' in expr:
                expr = '(%s)' % expr
            self.vars[varname] = '%s %s %s' % (expr, op, other)
        else:
            self.vars[varname] = '%s %s %s' % (varname, op, other)
            self.dependent_variables.add(varname)


# Read the file, simplify it in ONE pass
current_vars = None
with open('blockcipher.wasm2c.c', 'r') as fd:
    in_fct = False

    for line in fd:
        line = line.rstrip()
        sline = line.strip()
        indent = ' ' * (len(line) - len(sline))
        assert line.startswith(indent)

        if not in_fct:
            print(line)
            if sline == 'FUNC_PROLOGUE;':
                in_fct = True
                current_vars = CurrentVariables(indent=indent)
            continue

        if sline == 'FUNC_EPILOGUE;':
            current_vars.dump()
            print(line)
            in_fct = False
            continue
        assert sline != 'FUNC_PROLOGUE;'

        # Change of indent
        if indent != current_vars.indent:
            if len(indent) < len(current_vars.indent):
                current_vars.dump()
            else:
                assert not current_vars.vars, "current_vars needs to be empty when entering a block"
            current_vars.indent = indent

        # Assigment
        m = re.match(r'([a-z][0-9]+) = (.*);$', sline)
        if m is not None:
            varname, expr = m.groups()
            current_vars.assign(varname, expr)
            continue

        # Biop, only with over variable
        m = re.match(r'([a-z][0-9]+) ([+-^*&|])= ([a-z][0-9]+);$', sline)
        if m is not None:
            varname, op, other = m.groups()
            current_vars.update_with_biop(varname, op, other)
            continue

        # Store to memory
        m = re.match(r'i64_store\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+)\), ([a-z][0-9]+)\);$', sline)
        if m is not None:
            addr, value = m.groups()
            addr = current_vars.vars.get(addr, addr)
            value = current_vars.vars.get(value, value)
            current_vars.dump()
            print('%sMEM_as_u64[%s] := %s;' % (indent, addr, value))
            continue
        m = re.match(r'i32_store8\(Z_envZ_memory, \(u64\)\(([a-z][0-9]+)\), ([a-z][0-9]+)\);$', sline)
        if m is not None:
            addr, value = m.groups()
            addr = current_vars.vars.get(addr, addr)
            value = current_vars.vars.get(value, value)
            current_vars.dump()
            print('%sMEM_as_u8[%s] := %s;' % (indent, addr, value))
            continue

        # By default, print the line
        current_vars.dump()
        print(line)
