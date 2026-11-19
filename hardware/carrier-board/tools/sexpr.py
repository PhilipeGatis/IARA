"""Minimal S-expression reader/writer for KiCad files."""
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)


class Sym(str):
    """Bare (unquoted) atom."""


def parse(text):
    stack, cur = [], []
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError("bad token at %d" % pos)
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(m.group(3).replace('\\"', '"').replace("\\\\", "\\"))
        else:
            cur.append(Sym(m.group(4)))
    return cur[0] if len(cur) == 1 else cur


def _atom(a):
    if isinstance(a, Sym):
        return str(a)
    if isinstance(a, bool):
        return "yes" if a else "no"
    if isinstance(a, (int, float)):
        s = ("%.4f" % a).rstrip("0").rstrip(".") if isinstance(a, float) else str(a)
        return "0" if s in ("-0", "") else s
    s = str(a).replace("\\", "\\\\").replace('"', '\\"')
    return '"%s"' % s


def dump(e, indent=0):
    if not isinstance(e, list):
        return _atom(e)
    simple = all(not isinstance(x, list) for x in e)
    if simple:
        return "(" + " ".join(_atom(x) for x in e) + ")"
    pad = "\t" * (indent + 1)
    head = []
    rest = []
    for i, x in enumerate(e):
        if not isinstance(x, list) and not rest:
            head.append(_atom(x))
        else:
            rest.append(x)
    out = "(" + " ".join(head)
    for x in rest:
        out += "\n" + pad + dump(x, indent + 1)
    return out + "\n" + "\t" * indent + ")"


def find(e, key):
    return [x for x in e if isinstance(x, list) and x and x[0] == key]


def find1(e, key):
    r = find(e, key)
    return r[0] if r else None
