.. title:: clang-tidy - readability-visible-exception-propagation

readability-visible-exception-propagation
=========================================

Issues warnings for call sites of throwing functions that are not annotaed [[clang::maybe_unhandled]].
Issues warnings for call sites of noexcept functions that are annotated [[clang::maybe_unhandled]].

.. code-block:: c++

void foo();
void bar() noexcept;

void baz()
{
    foo(); // warning
    [[try]] foo(); // no warning
    bar(); // no warning
    [[try]] bar(); // warning
}