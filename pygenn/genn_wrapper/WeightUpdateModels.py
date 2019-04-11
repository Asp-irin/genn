# This file was automatically generated by SWIG (http://www.swig.org).
# Version 3.0.12
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.

from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    def swig_import_helper():
        import importlib
        pkg = __name__.rpartition('.')[0]
        mname = '.'.join((pkg, '_WeightUpdateModels')).lstrip('.')
        try:
            return importlib.import_module(mname)
        except ImportError:
            return importlib.import_module('_WeightUpdateModels')
    _WeightUpdateModels = swig_import_helper()
    del swig_import_helper
elif _swig_python_version_info >= (2, 6, 0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_WeightUpdateModels', [dirname(__file__)])
        except ImportError:
            import _WeightUpdateModels
            return _WeightUpdateModels
        try:
            _mod = imp.load_module('_WeightUpdateModels', fp, pathname, description)
        finally:
            if fp is not None:
                fp.close()
        return _mod
    _WeightUpdateModels = swig_import_helper()
    del swig_import_helper
else:
    import _WeightUpdateModels
del _swig_python_version_info

try:
    _swig_property = property
except NameError:
    pass  # Python < 2.2 doesn't have 'property'.

try:
    import builtins as __builtin__
except ImportError:
    import __builtin__

def _swig_setattr_nondynamic(self, class_type, name, value, static=1):
    if (name == "thisown"):
        return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name, None)
    if method:
        return method(self, value)
    if (not static):
        if _newclass:
            object.__setattr__(self, name, value)
        else:
            self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)


def _swig_setattr(self, class_type, name, value):
    return _swig_setattr_nondynamic(self, class_type, name, value, 0)


def _swig_getattr(self, class_type, name):
    if (name == "thisown"):
        return self.this.own()
    method = class_type.__swig_getmethods__.get(name, None)
    if method:
        return method(self)
    raise AttributeError("'%s' object has no attribute '%s'" % (class_type.__name__, name))


def _swig_repr(self):
    try:
        strthis = "proxy of " + self.this.__repr__()
    except __builtin__.Exception:
        strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except __builtin__.Exception:
    class _object:
        pass
    _newclass = 0

try:
    import weakref
    weakref_proxy = weakref.proxy
except __builtin__.Exception:
    weakref_proxy = lambda x: x


from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    from . import NewModels
else:
    import NewModels
del _swig_python_version_info
from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    from . import Snippet
else:
    import Snippet
del _swig_python_version_info
from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    from . import StlContainers
else:
    import StlContainers
del _swig_python_version_info
from sys import version_info as _swig_python_version_info
if _swig_python_version_info >= (2, 7, 0):
    from . import InitVarSnippet
else:
    import InitVarSnippet
del _swig_python_version_info
class Base(NewModels.Base):
    __swig_setmethods__ = {}
    for _s in [NewModels.Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, Base, name, value)
    __swig_getmethods__ = {}
    for _s in [NewModels.Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, Base, name)
    __repr__ = _swig_repr

    def get_sim_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_sim_code(self)

    def get_event_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_event_code(self)

    def get_learn_post_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_learn_post_code(self)

    def get_synapse_dynamics_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_synapse_dynamics_code(self)

    def get_event_threshold_condition_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_event_threshold_condition_code(self)

    def get_sim_support_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_sim_support_code(self)

    def get_learn_post_support_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_learn_post_support_code(self)

    def get_synapse_dynamics_suppport_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_synapse_dynamics_suppport_code(self)

    def get_pre_spike_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_pre_spike_code(self)

    def get_post_spike_code(self) -> "std::string":
        return _WeightUpdateModels.Base_get_post_spike_code(self)

    def get_pre_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.Base_get_pre_vars(self)

    def get_post_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.Base_get_post_vars(self)

    def get_extra_global_params(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.Base_get_extra_global_params(self)

    def is_pre_spike_time_required(self) -> "bool":
        return _WeightUpdateModels.Base_is_pre_spike_time_required(self)

    def is_post_spike_time_required(self) -> "bool":
        return _WeightUpdateModels.Base_is_post_spike_time_required(self)

    def get_pre_var_index(self, varName: 'std::string const &') -> "size_t":
        return _WeightUpdateModels.Base_get_pre_var_index(self, varName)

    def get_post_var_index(self, varName: 'std::string const &') -> "size_t":
        return _WeightUpdateModels.Base_get_post_var_index(self, varName)

    def __init__(self):
        if self.__class__ == Base:
            _self = None
        else:
            _self = self
        this = _WeightUpdateModels.new_Base(_self, )
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_Base
    __del__ = lambda self: None
    def __disown__(self):
        self.this.disown()
        _WeightUpdateModels.disown_Base(self)
        return weakref_proxy(self)
Base_swigregister = _WeightUpdateModels.Base_swigregister
Base_swigregister(Base)

class StaticPulse(Base):
    __swig_setmethods__ = {}
    for _s in [Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, StaticPulse, name, value)
    __swig_getmethods__ = {}
    for _s in [Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, StaticPulse, name)
    __repr__ = _swig_repr
    if _newclass:
        get_instance = staticmethod(_WeightUpdateModels.StaticPulse_get_instance)
    else:
        get_instance = _WeightUpdateModels.StaticPulse_get_instance

    def get_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.StaticPulse_get_vars(self)

    def get_sim_code(self) -> "std::string":
        return _WeightUpdateModels.StaticPulse_get_sim_code(self)
    if _newclass:
        make_param_values = staticmethod(_WeightUpdateModels.StaticPulse_make_param_values)
    else:
        make_param_values = _WeightUpdateModels.StaticPulse_make_param_values
    if _newclass:
        make_var_values = staticmethod(_WeightUpdateModels.StaticPulse_make_var_values)
    else:
        make_var_values = _WeightUpdateModels.StaticPulse_make_var_values
    if _newclass:
        make_pre_var_values = staticmethod(_WeightUpdateModels.StaticPulse_make_pre_var_values)
    else:
        make_pre_var_values = _WeightUpdateModels.StaticPulse_make_pre_var_values
    if _newclass:
        make_post_var_values = staticmethod(_WeightUpdateModels.StaticPulse_make_post_var_values)
    else:
        make_post_var_values = _WeightUpdateModels.StaticPulse_make_post_var_values

    def __init__(self):
        this = _WeightUpdateModels.new_StaticPulse()
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_StaticPulse
    __del__ = lambda self: None
StaticPulse_swigregister = _WeightUpdateModels.StaticPulse_swigregister
StaticPulse_swigregister(StaticPulse)

def StaticPulse_get_instance() -> "WeightUpdateModels::StaticPulse const *":
    return _WeightUpdateModels.StaticPulse_get_instance()
StaticPulse_get_instance = _WeightUpdateModels.StaticPulse_get_instance

def StaticPulse_make_param_values(arg2: 'DoubleVector') -> "WeightUpdateModels::StaticPulse::ParamValues *":
    return _WeightUpdateModels.StaticPulse_make_param_values(arg2)
StaticPulse_make_param_values = _WeightUpdateModels.StaticPulse_make_param_values

def StaticPulse_make_var_values(vals: 'VarInitVector') -> "WeightUpdateModels::StaticPulse::VarValues *":
    return _WeightUpdateModels.StaticPulse_make_var_values(vals)
StaticPulse_make_var_values = _WeightUpdateModels.StaticPulse_make_var_values

def StaticPulse_make_pre_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::StaticPulse::PreVarValues *":
    return _WeightUpdateModels.StaticPulse_make_pre_var_values(arg2)
StaticPulse_make_pre_var_values = _WeightUpdateModels.StaticPulse_make_pre_var_values

def StaticPulse_make_post_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::StaticPulse::PostVarValues *":
    return _WeightUpdateModels.StaticPulse_make_post_var_values(arg2)
StaticPulse_make_post_var_values = _WeightUpdateModels.StaticPulse_make_post_var_values

class StaticPulseDendriticDelay(Base):
    __swig_setmethods__ = {}
    for _s in [Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, StaticPulseDendriticDelay, name, value)
    __swig_getmethods__ = {}
    for _s in [Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, StaticPulseDendriticDelay, name)
    __repr__ = _swig_repr
    if _newclass:
        get_instance = staticmethod(_WeightUpdateModels.StaticPulseDendriticDelay_get_instance)
    else:
        get_instance = _WeightUpdateModels.StaticPulseDendriticDelay_get_instance

    def get_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.StaticPulseDendriticDelay_get_vars(self)

    def get_sim_code(self) -> "std::string":
        return _WeightUpdateModels.StaticPulseDendriticDelay_get_sim_code(self)
    if _newclass:
        make_param_values = staticmethod(_WeightUpdateModels.StaticPulseDendriticDelay_make_param_values)
    else:
        make_param_values = _WeightUpdateModels.StaticPulseDendriticDelay_make_param_values
    if _newclass:
        make_var_values = staticmethod(_WeightUpdateModels.StaticPulseDendriticDelay_make_var_values)
    else:
        make_var_values = _WeightUpdateModels.StaticPulseDendriticDelay_make_var_values

    def __init__(self):
        this = _WeightUpdateModels.new_StaticPulseDendriticDelay()
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_StaticPulseDendriticDelay
    __del__ = lambda self: None
StaticPulseDendriticDelay_swigregister = _WeightUpdateModels.StaticPulseDendriticDelay_swigregister
StaticPulseDendriticDelay_swigregister(StaticPulseDendriticDelay)

def StaticPulseDendriticDelay_get_instance() -> "WeightUpdateModels::StaticPulseDendriticDelay const *":
    return _WeightUpdateModels.StaticPulseDendriticDelay_get_instance()
StaticPulseDendriticDelay_get_instance = _WeightUpdateModels.StaticPulseDendriticDelay_get_instance

def StaticPulseDendriticDelay_make_param_values(arg2: 'DoubleVector') -> "WeightUpdateModels::StaticPulseDendriticDelay::ParamValues *":
    return _WeightUpdateModels.StaticPulseDendriticDelay_make_param_values(arg2)
StaticPulseDendriticDelay_make_param_values = _WeightUpdateModels.StaticPulseDendriticDelay_make_param_values

def StaticPulseDendriticDelay_make_var_values(vals: 'VarInitVector') -> "WeightUpdateModels::StaticPulseDendriticDelay::VarValues *":
    return _WeightUpdateModels.StaticPulseDendriticDelay_make_var_values(vals)
StaticPulseDendriticDelay_make_var_values = _WeightUpdateModels.StaticPulseDendriticDelay_make_var_values

class StaticGraded(Base):
    __swig_setmethods__ = {}
    for _s in [Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, StaticGraded, name, value)
    __swig_getmethods__ = {}
    for _s in [Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, StaticGraded, name)
    __repr__ = _swig_repr
    if _newclass:
        get_instance = staticmethod(_WeightUpdateModels.StaticGraded_get_instance)
    else:
        get_instance = _WeightUpdateModels.StaticGraded_get_instance

    def get_param_names(self) -> "Snippet::Base::StringVec":
        return _WeightUpdateModels.StaticGraded_get_param_names(self)

    def get_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.StaticGraded_get_vars(self)

    def get_event_code(self) -> "std::string":
        return _WeightUpdateModels.StaticGraded_get_event_code(self)

    def get_event_threshold_condition_code(self) -> "std::string":
        return _WeightUpdateModels.StaticGraded_get_event_threshold_condition_code(self)
    if _newclass:
        make_param_values = staticmethod(_WeightUpdateModels.StaticGraded_make_param_values)
    else:
        make_param_values = _WeightUpdateModels.StaticGraded_make_param_values
    if _newclass:
        make_var_values = staticmethod(_WeightUpdateModels.StaticGraded_make_var_values)
    else:
        make_var_values = _WeightUpdateModels.StaticGraded_make_var_values
    if _newclass:
        make_pre_var_values = staticmethod(_WeightUpdateModels.StaticGraded_make_pre_var_values)
    else:
        make_pre_var_values = _WeightUpdateModels.StaticGraded_make_pre_var_values
    if _newclass:
        make_post_var_values = staticmethod(_WeightUpdateModels.StaticGraded_make_post_var_values)
    else:
        make_post_var_values = _WeightUpdateModels.StaticGraded_make_post_var_values

    def __init__(self):
        this = _WeightUpdateModels.new_StaticGraded()
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_StaticGraded
    __del__ = lambda self: None
StaticGraded_swigregister = _WeightUpdateModels.StaticGraded_swigregister
StaticGraded_swigregister(StaticGraded)

def StaticGraded_get_instance() -> "WeightUpdateModels::StaticGraded const *":
    return _WeightUpdateModels.StaticGraded_get_instance()
StaticGraded_get_instance = _WeightUpdateModels.StaticGraded_get_instance

def StaticGraded_make_param_values(vals: 'DoubleVector') -> "WeightUpdateModels::StaticGraded::ParamValues *":
    return _WeightUpdateModels.StaticGraded_make_param_values(vals)
StaticGraded_make_param_values = _WeightUpdateModels.StaticGraded_make_param_values

def StaticGraded_make_var_values(vals: 'VarInitVector') -> "WeightUpdateModels::StaticGraded::VarValues *":
    return _WeightUpdateModels.StaticGraded_make_var_values(vals)
StaticGraded_make_var_values = _WeightUpdateModels.StaticGraded_make_var_values

def StaticGraded_make_pre_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::StaticGraded::PreVarValues *":
    return _WeightUpdateModels.StaticGraded_make_pre_var_values(arg2)
StaticGraded_make_pre_var_values = _WeightUpdateModels.StaticGraded_make_pre_var_values

def StaticGraded_make_post_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::StaticGraded::PostVarValues *":
    return _WeightUpdateModels.StaticGraded_make_post_var_values(arg2)
StaticGraded_make_post_var_values = _WeightUpdateModels.StaticGraded_make_post_var_values

class PiecewiseSTDP(Base):
    __swig_setmethods__ = {}
    for _s in [Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, PiecewiseSTDP, name, value)
    __swig_getmethods__ = {}
    for _s in [Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, PiecewiseSTDP, name)
    __repr__ = _swig_repr
    if _newclass:
        get_instance = staticmethod(_WeightUpdateModels.PiecewiseSTDP_get_instance)
    else:
        get_instance = _WeightUpdateModels.PiecewiseSTDP_get_instance

    def get_param_names(self) -> "Snippet::Base::StringVec":
        return _WeightUpdateModels.PiecewiseSTDP_get_param_names(self)

    def get_vars(self) -> "Snippet::Base::StringPairVec":
        return _WeightUpdateModels.PiecewiseSTDP_get_vars(self)

    def get_sim_code(self) -> "std::string":
        return _WeightUpdateModels.PiecewiseSTDP_get_sim_code(self)

    def get_learn_post_code(self) -> "std::string":
        return _WeightUpdateModels.PiecewiseSTDP_get_learn_post_code(self)

    def get_derived_params(self) -> "Snippet::Base::DerivedParamVec":
        return _WeightUpdateModels.PiecewiseSTDP_get_derived_params(self)

    def is_pre_spike_time_required(self) -> "bool":
        return _WeightUpdateModels.PiecewiseSTDP_is_pre_spike_time_required(self)

    def is_post_spike_time_required(self) -> "bool":
        return _WeightUpdateModels.PiecewiseSTDP_is_post_spike_time_required(self)
    if _newclass:
        make_param_values = staticmethod(_WeightUpdateModels.PiecewiseSTDP_make_param_values)
    else:
        make_param_values = _WeightUpdateModels.PiecewiseSTDP_make_param_values
    if _newclass:
        make_var_values = staticmethod(_WeightUpdateModels.PiecewiseSTDP_make_var_values)
    else:
        make_var_values = _WeightUpdateModels.PiecewiseSTDP_make_var_values
    if _newclass:
        make_pre_var_values = staticmethod(_WeightUpdateModels.PiecewiseSTDP_make_pre_var_values)
    else:
        make_pre_var_values = _WeightUpdateModels.PiecewiseSTDP_make_pre_var_values
    if _newclass:
        make_post_var_values = staticmethod(_WeightUpdateModels.PiecewiseSTDP_make_post_var_values)
    else:
        make_post_var_values = _WeightUpdateModels.PiecewiseSTDP_make_post_var_values

    def __init__(self):
        this = _WeightUpdateModels.new_PiecewiseSTDP()
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_PiecewiseSTDP
    __del__ = lambda self: None
PiecewiseSTDP_swigregister = _WeightUpdateModels.PiecewiseSTDP_swigregister
PiecewiseSTDP_swigregister(PiecewiseSTDP)

def PiecewiseSTDP_get_instance() -> "WeightUpdateModels::PiecewiseSTDP const *":
    return _WeightUpdateModels.PiecewiseSTDP_get_instance()
PiecewiseSTDP_get_instance = _WeightUpdateModels.PiecewiseSTDP_get_instance

def PiecewiseSTDP_make_param_values(vals: 'DoubleVector') -> "WeightUpdateModels::PiecewiseSTDP::ParamValues *":
    return _WeightUpdateModels.PiecewiseSTDP_make_param_values(vals)
PiecewiseSTDP_make_param_values = _WeightUpdateModels.PiecewiseSTDP_make_param_values

def PiecewiseSTDP_make_var_values(vals: 'VarInitVector') -> "WeightUpdateModels::PiecewiseSTDP::VarValues *":
    return _WeightUpdateModels.PiecewiseSTDP_make_var_values(vals)
PiecewiseSTDP_make_var_values = _WeightUpdateModels.PiecewiseSTDP_make_var_values

def PiecewiseSTDP_make_pre_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::PiecewiseSTDP::PreVarValues *":
    return _WeightUpdateModels.PiecewiseSTDP_make_pre_var_values(arg2)
PiecewiseSTDP_make_pre_var_values = _WeightUpdateModels.PiecewiseSTDP_make_pre_var_values

def PiecewiseSTDP_make_post_var_values(arg2: 'VarInitVector') -> "WeightUpdateModels::PiecewiseSTDP::PostVarValues *":
    return _WeightUpdateModels.PiecewiseSTDP_make_post_var_values(arg2)
PiecewiseSTDP_make_post_var_values = _WeightUpdateModels.PiecewiseSTDP_make_post_var_values

class Custom(Base):
    __swig_setmethods__ = {}
    for _s in [Base]:
        __swig_setmethods__.update(getattr(_s, '__swig_setmethods__', {}))
    __setattr__ = lambda self, name, value: _swig_setattr(self, Custom, name, value)
    __swig_getmethods__ = {}
    for _s in [Base]:
        __swig_getmethods__.update(getattr(_s, '__swig_getmethods__', {}))
    __getattr__ = lambda self, name: _swig_getattr(self, Custom, name)
    __repr__ = _swig_repr
    if _newclass:
        get_instance = staticmethod(_WeightUpdateModels.Custom_get_instance)
    else:
        get_instance = _WeightUpdateModels.Custom_get_instance
    if _newclass:
        make_param_values = staticmethod(_WeightUpdateModels.Custom_make_param_values)
    else:
        make_param_values = _WeightUpdateModels.Custom_make_param_values
    if _newclass:
        make_var_values = staticmethod(_WeightUpdateModels.Custom_make_var_values)
    else:
        make_var_values = _WeightUpdateModels.Custom_make_var_values
    if _newclass:
        make_pre_var_values = staticmethod(_WeightUpdateModels.Custom_make_pre_var_values)
    else:
        make_pre_var_values = _WeightUpdateModels.Custom_make_pre_var_values
    if _newclass:
        make_post_var_values = staticmethod(_WeightUpdateModels.Custom_make_post_var_values)
    else:
        make_post_var_values = _WeightUpdateModels.Custom_make_post_var_values

    def __init__(self):
        if self.__class__ == Custom:
            _self = None
        else:
            _self = self
        this = _WeightUpdateModels.new_Custom(_self, )
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this
    __swig_destroy__ = _WeightUpdateModels.delete_Custom
    __del__ = lambda self: None
    def __disown__(self):
        self.this.disown()
        _WeightUpdateModels.disown_Custom(self)
        return weakref_proxy(self)
Custom_swigregister = _WeightUpdateModels.Custom_swigregister
Custom_swigregister(Custom)

def Custom_get_instance() -> "WeightUpdateModels::Custom const *":
    return _WeightUpdateModels.Custom_get_instance()
Custom_get_instance = _WeightUpdateModels.Custom_get_instance

def Custom_make_param_values(vals: 'DoubleVector') -> "CustomValues::ParamValues *":
    return _WeightUpdateModels.Custom_make_param_values(vals)
Custom_make_param_values = _WeightUpdateModels.Custom_make_param_values

def Custom_make_var_values(vals: 'VarInitVector') -> "CustomValues::VarValues *":
    return _WeightUpdateModels.Custom_make_var_values(vals)
Custom_make_var_values = _WeightUpdateModels.Custom_make_var_values

def Custom_make_pre_var_values(vals: 'VarInitVector') -> "CustomValues::VarValues *":
    return _WeightUpdateModels.Custom_make_pre_var_values(vals)
Custom_make_pre_var_values = _WeightUpdateModels.Custom_make_pre_var_values

def Custom_make_post_var_values(vals: 'VarInitVector') -> "CustomValues::VarValues *":
    return _WeightUpdateModels.Custom_make_post_var_values(vals)
Custom_make_post_var_values = _WeightUpdateModels.Custom_make_post_var_values

# This file is compatible with both classic and new-style classes.


