#!/usr/bin/env julia

# We'll be passing structs within structs, so this guy is useful to have around
using StrPack

# Get our compiler runtime library so that we know magic constants like NSConcreteStackBlock
libcompilerrt = dlopen("/usr/lib/system/libcompiler_rt.dylib")

# Block_descriptor describes the signature of the wrapped function.  Note that we
# don't deal with any of the "fancy" features, such as helper initialization functions
@struct type Block_descriptor
    # reserved is always zero
    reserved::Culong

    # size is always sizeof(Block_descriptor), as we never deal with helper functions
    size::Culong

    # signature is a string describing the return and argument types of the block
    # See Objective-C Runtime Type Encodings for more information regarding this
    signature::Ptr{Cchar}
end
Block_descriptor(signature) = Block_descriptor(0, sizeof(Block_descriptor), pointer(signature))


# Block_literal contains everything you need to know about this block
@struct type Block_literal
    # isa is a magic constant taht we always set to NSConcreteStackBlock
    isa::Ptr{Void}

    # flags tells information about the descriptor; we will always use the simplest
    # descriptor format available; all it has is a signature; no helper functions
    flags::Cint

    # reserved is always zero
    reserved::Cint

    # Pointer to actual function address
    invoke::Ptr{Void}

    # Pointer to descriptor struct
    descriptor::Ptr{Void}
end

# Constructing a Block_literal just a tiny bit more involved than constructing
# a Block_descriptor.  We need to wrap the passed func as a cfunction, so we
# do so in creating the `invoke` pointer, but since I don't yet know how to
# infer the Cint return type and nonexistant arguments from `func`, we hardcode
# them in.  We then construct a Block_descriptor, pack it, and then return the
# whole object, which itself is then ready to be packed and passed to a function
function Block_literal(func, signature)
    # Thank god for dlsym()
    isa = dlsym(libcompilerrt, :_NSConcreteStackBlock)

    # 1 << 30 is the magic flag for BLOCK_HAS_SIGNATURE
    flags = (1 << 30)
    reserved = 0

    # pack invoke
    invoke = cfunction(func, Cint, ())

    # pack the descriptor
    desc = Block_descriptor(signature)
    iostr = IOBuffer()
    pack(iostr, desc)

    return Block_literal(isa, flags, reserved, invoke, iostr.data)
end

# This goes from func/signature all the way to packed Block_literal
function make_block(func, signature)
    block = Block_literal(func, signature)
    iostr = IOBuffer()
    pack(iostr, block)

    return iostr.data
end





# This is my callback function that will be wrapped inside a block
# and called form objective-c++ code!  So cool! 
function foo()
    println("This is foo!")

    # Unfortunately, Cint(5) doesn't work; we have to use int32(5)
    return int32(5)
end

# Let's make a block, then strpack it!  We hardcode the signature in here:
block = make_block(foo, "i8@?0")

# let's now call runBlock() from libblocktest
libblocktest = dlopen("./libblocktest.dylib")
ccall((:runBlock, "libblocktest"), Void, (Ptr{Void},), block)

# Expected output:
#
# $ julia blocktest.jl 
# About to run theBlock
# This is foo!
# Just ran theBlock with result: 5
