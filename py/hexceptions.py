class HeraclesException(Exception):
    pass

class HeraclesNoMemError(HeraclesException):
    pass

class HeraclesInternalError(HeraclesException):
    pass

class HeraclesPathXError(HeraclesException):
    pass

class HeraclesNoMatchError(HeraclesException):
    pass

class HeraclesMMatchError(HeraclesException):
    pass

class HeraclesSyntaxError(HeraclesException):
    pass

class HeraclesNoLensError(HeraclesException):
    pass

class HeraclesMXFMError(HeraclesException):
    pass

class HeraclesNoSpanError(HeraclesException):
    pass

class HeraclesMvDescError(HeraclesException):
    pass

class HeraclesCmdRunError(HeraclesException):
    pass

class HeraclesBadArgError(HeraclesException):
    pass

class HeraclesLabelError(HeraclesException):
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
