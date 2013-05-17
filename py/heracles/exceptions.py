class HeraclesError(Exception):
    pass

class HeraclesNoMemError(HeraclesError):
    pass

class HeraclesInternalError(HeraclesError):
    pass

class HeraclesPathXError(HeraclesError):
    pass

class HeraclesNoMatchError(HeraclesError):
    pass

class HeraclesMMatchError(HeraclesError):
    pass

class HeraclesSyntaxError(HeraclesError):
    pass

class HeraclesNoLensError(HeraclesError):
    pass

class HeraclesMXFMError(HeraclesError):
    pass

class HeraclesNoSpanError(HeraclesError):
    pass

class HeraclesMvDescError(HeraclesError):
    pass

class HeraclesCmdRunError(HeraclesError):
    pass

class HeraclesBadArgError(HeraclesError):
    pass

class HeraclesLabelError(HeraclesError):
    pass

exception_list = (None, HeraclesNoMemError, HeraclesNoMemError, 
        HeraclesInternalError, HeraclesPathXError, HeraclesNoMatchError, 
        HeraclesMMatchError, HeraclesSyntaxError, HeraclesNoLensError, 
        HeraclesMXFMError, HeraclesNoSpanError, HeraclesMvDescError, 
        HeraclesCmdRunError, HeraclesBadArgError, HeraclesLabelError)

def get_exception(heracles):
    error = heracles.error
    code = int(error.code)
    exception = exception_list[code]
    if exception is not None:
        details = str(error.details)
        
        return exception(details)

# Tree exceptions

class HeraclesTreeError(HeraclesError):
    pass

class HeraclesTreeLabelError(HeraclesTreeError):
    pass

