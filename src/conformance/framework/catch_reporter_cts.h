// Copyright (c) 2019-2023, The Khronos Group Inc.
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#ifndef CATCH_REPORTER_CTS_H_INCLUDED
#define CATCH_REPORTER_CTS_H_INCLUDED

// Based on the upstream JunitReporter

#include "utilities/utils.h"

XRC_DISABLE_MSVC_WARNING(4324)

#include <catch2/reporters/catch_reporter_cumulative_base.hpp>
#include <catch2/internal/catch_xmlwriter.hpp>
#include <catch2/catch_timer.hpp>
#include "catch2/interfaces/catch_interfaces_reporter_factory.hpp"

namespace Catch
{

    IReporterFactoryPtr makeCtsReporterFactory();

    /// Based on the upstream JunitReporter
    class CTSReporter final : public CumulativeReporterBase
    {
    public:
        CTSReporter(ReporterConfig&& _config);

        ~CTSReporter() override;

        static std::string getDescription();

        void testRunStarting(TestRunInfo const& runInfo) override;

        void testCaseStarting(TestCaseInfo const& testCaseInfo) override;
        void assertionEnded(AssertionStats const& assertionStats) override;

        void testCaseEnded(TestCaseStats const& testCaseStats) override;

        void testRunEndedCumulative() override;

    private:
        void writeRun(TestRunNode const& testRunNode, double suiteTime);

        void writeTestCase(TestCaseNode const& testCaseNode);

        void writeSection(std::string const& className, std::string const& rootName, SectionNode const& sectionNode, bool testOkToFail);

        void writeAssertions(SectionNode const& sectionNode);
        void writeAssertion(AssertionStats const& stats);

        XmlWriter xml;
        Timer suiteTimer;
        std::string stdOutForSuite;
        std::string stdErrForSuite;
        unsigned int unexpectedExceptions = 0;
        bool m_okToFail = false;
    };

}  // end namespace Catch

#endif  // CATCH_REPORTER_CTS_H_INCLUDED
