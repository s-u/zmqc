\name{zmqc}
\alias{zmqc}
\title{
   0MQ PUB-SUB connection
}
\description{
  \code{zmqc} creates a 0MQ PUB/SUB connection. Modes beginning with
  "w" and "a" are equivalent and create a PUB connection while modes
  beginning with "r" create a SUB connection.
}
\usage{
  zmqc(endpoint, mode = "")
}
\arguments{
  \item{endpoint}{string defining the 0MQ endpoint of the connection
    consisting of \code{<transport>://<address>}. See 0MQ documentation
    for details, but most common transports are \code{tcp}, \code{ipc},
    \code{inproc}, \code{pgm} and \code{epgm}.
  }
  \item{mode}{see Modes section in connections documentation and below.}
}
\details{
  \code{mode} set to \code{"r"} or \code{"rb"} opens a SUB connection
  and it is read-only by design. \code{mode} set to \code{"w"} or
  \code{"wb"} creates a PUB connection which is write-only by
  design. The modes \code{"a"} and \code{"ab"} are equivalent to
  \code{"w"} and \code{"wb"} respectively. No other modes are
  supported.
}
\value{
  zmqc connection
}
%\references{
%}
\author{
  Simon Urbanek
}
%\seealso{
%}
\examples{
  pub <- zmqc("ipc:///tmp/0mq.sock", "w")
  sub <- zmqc("ipc:///tmp/0mq.sock", "r")
  writeLines("hello, world!", pub)
  writeLines("I'm publishing ...", pub)
  readLines(sub, 1)
  close(sub)
  close(pub)
}
\keyword{connection}
