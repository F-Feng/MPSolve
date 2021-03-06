/*
 * This file is part of MPSolve 3.1.5
 *
 * Copyright (C) 2001-2015, Dipartimento di Matematica "L. Tonelli", Pisa.
 * License: http://www.gnu.org/licenses/gpl.html GPL version 3 or higher
 *
 * Authors:
 *   Leonardo Robol <leonardo.robol@sns.it>
 */

/* This is required for old glibc */
#define _GNU_SOURCE

#include <mps/mps.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

/* Internal states of the parser */
#define PARSING_SIGN        1
#define PARSING_COEFFICIENT 2
#define PARSING_EXPONENT    3
#define PARSING_ERROR        4
#define PARSING_RESET        5

static char * parse_sign (mps_context * ctx, char * line, int * sign, mps_boolean *sign_found);

#ifndef HAVE_FMEMOPEN
/* Forward declaration. Implementation of this function, in case is not provided by the
 * system, is located in input-output.c */
FILE * fmemopen (void *buf, size_t size, const char *mode);;
#endif

static char * find_fp_separator (mps_context  * ctx, char * line)
{
  while (!isspace (*line) && *line != '\0')
    {
      if (*line == '.')
        return line;

      line++;
    }

  return NULL;
}

static long int
parse_fp_exponent (mps_context * ctx, char * exponent_start)
{
  char * ptr = exponent_start;
  char * copy = NULL;

  while (*ptr != 'x' && *ptr != '\0')
    ptr++;

  copy = mps_newv (char, ptr - exponent_start + 1);
  strncpy (copy, exponent_start, ptr - exponent_start);
  *(copy + (ptr - exponent_start)) = '\0';

  long int response = strtol (copy, &ptr, 10);

  if (*ptr != '\0')
    mps_error (ctx, "Error parsing exponent of coefficient: %s", copy);

  free (copy);

  return response;
}

MPS_PRIVATE
char * build_equivalent_rational_string (mps_context * ctx, const char * orig_line, long int * exponent, int * sign)
{
  int i;
  char * line = NULL;

  /* This an over estimate of the length of the _real_ token that we
   * have to parse but, given that we use mps_input_buffer to perform
   * the tokenization of the input, it will not hopefully be a "big"
   * over-estimate. */
  char * allocated_line = strdup (orig_line);
  char * copy = mps_newv (char, 2 * strlen (orig_line) + 5);
  char * copy_ptr = copy;
  char * line_ptr = allocated_line;
  char * line_end = allocated_line + strlen (orig_line);
  long int denominator = 0;
  mps_boolean dot_found = false;
  mps_boolean sign_found = false;

  line = allocated_line;

  char * sep = find_fp_separator (ctx, line);

  /* Note that a string could have a prepended sign */
  line = parse_sign (ctx, line, sign, &sign_found);
  
  /* Scan the string and truncate it if necessary */
  while (line_ptr++ < line_end)
    {
      if ((*line_ptr == '+' || *line_ptr == '-') &&
          (*(line_ptr - 1) != 'e' && *(line_ptr - 1) != 'E'))
        *line_ptr = '\0';
    }

  line_ptr = line;
  line_end = line_ptr + strlen (line);

  /* If we have floating point input and also a rational
   * separator raise an error. */
  if ((sep || strchr (line, 'e') || strchr (line, 'E'))
      && (strchr (line, '/') != NULL))
    {
      free (line);
      free (copy);
      return NULL;
    }

  *exponent = 0;

  {
    while (line_ptr < line_end)
      {
        /* Check that we don't meet 'e' or 'E'. Otherwise we should exit
         * and parse the exponent instead. */
        if (*line_ptr == 'e' || *line_ptr == 'E')
          {
            *exponent = parse_fp_exponent (ctx, line_ptr + 1);
            break;
          }

        if (*line_ptr == 'x' || *line_ptr == '+' || *line_ptr == '-')
          break;

        if (*line_ptr == '.')
          {
            dot_found = true;
            line_ptr++;
          }

        if (dot_found)
          denominator++;

        *copy_ptr = *line_ptr;
        line_ptr++;
        copy_ptr++;
      }

    /* TODO: Add the denominator part */
    if (denominator)
      {
        *copy_ptr++ = '/';
        *copy_ptr++ = '1';

        for (i = 0; i < denominator; i++)
          {
            *copy_ptr++ = '0';
          }
      }

    /* Close the token */
    *copy_ptr = '\0';
  }

  /* Clean the x^D part */
  for (copy_ptr = copy; copy_ptr < copy + strlen (copy); copy_ptr++)
    {
      if (*copy_ptr == 'x')
        {
          *copy_ptr = '\0';
          break;
        }
    }

  if (allocated_line)
    free (allocated_line);

  return copy;
}

static char *
parse_real_coefficient (mps_context * ctx, char * line, mpq_t coefficient)
{
  size_t offset = 0;
  long int exponent;
  int i;
  int sign = 1;
  const char * starting_line = line;

  if (*line == 'x')
    {
      mpq_set_ui (coefficient, 1U, 1U);
      return line;
    }
  else
    {
      mpq_t ten;

      mpq_init (ten);
      mpq_set_si (ten, 10, 1);

      char * coeff_line = build_equivalent_rational_string (ctx, line, &exponent, &sign);
      MPS_DEBUG_WITH_IO (ctx, "Transformed %s into %s", line, coeff_line);

      if (!coeff_line)
        {
          mps_error (ctx, "Cannot parse token: %s", line);
          return NULL;
        }

      offset = mpq_set_str (coefficient, coeff_line, 10);
      mpq_canonicalize (coefficient);

      if (sign == -1)
        mpq_neg (coefficient, coefficient);

      for (i = 0; i < exponent; i++)
        mpq_mul (coefficient, coefficient, ten);

      for (i = 0; i > exponent; i--)
        mpq_div (coefficient, coefficient, ten);

      if (offset != 0)
        {
          mps_error (ctx, "Cannot parse the coefficient: %s", line);
        }

      /* Consume the rest of the coefficient */
      while (isdigit (*line) || *line == '.' || *line == '/' || *line == 'e'
             || *line == 'E' ||
             (
               (*line == '-' || *line == '+') &&
               (
                 (line > starting_line) &&
                 (*(line - 1) == 'e' || *(line - 1) == 'E')
               )
             )
             )
        {
          line++;
        }

      mpq_clear (ten);

      free (coeff_line);
    }

  return line;
}

static char *
parse_complex_coefficient (mps_context * ctx, char *line, mpq_t coefficient_real,
                           mpq_t coefficient_imag)
{
  /* Here we break the coefficients in the real and imaginary part and
   * we call parse_real_coefficient() for both. */

  /* Detect the pieces that are required for the syntax of the complex
   * coefficients, i.e., the starting (, the comma, and the closing bracket. */
  char * starting_bracket = strchr (line, '(');
  char * comma = strchr (line, ',');
  char * closing_bracket = strchr (line, ')');

  mps_boolean success = false;

  /* Sanity checks here */
  if (starting_bracket == NULL)
    {
      mps_error (ctx, "Cannot find starting bracket for the complex coefficient");
      return NULL;
    }

  if (closing_bracket == NULL)
    {
      mps_error (ctx, "Cannot find the closing bracket for the complex coefficient");
      return NULL;
    }

  if (comma == NULL || comma > closing_bracket || comma < starting_bracket)
    {
      mps_error (ctx, "Missing or misplaced comma in the complex coefficient");
      return NULL;
    }

  char *real_part = mps_strndup (starting_bracket + 1, comma - starting_bracket - 1);
  char *imag_part = mps_strndup (comma + 1, closing_bracket - comma - 1);

  MPS_DEBUG_WITH_IO (ctx, "Extracted real part: %s", real_part);
  MPS_DEBUG_WITH_IO (ctx, "Extracted imaginary part: %s", imag_part);

  if (parse_real_coefficient (ctx, real_part, coefficient_real) == NULL)
    goto cleanup;

  if (parse_real_coefficient (ctx, imag_part, coefficient_imag) == NULL)
    goto cleanup;

  success = true;

cleanup:

  free (real_part);
  free (imag_part);

  return (success) ? closing_bracket + 1 : NULL;
}

static char *
parse_exponent (mps_context * ctx, char * line, int * degree)
{
  MPS_DEBUG_WITH_IO (ctx, "Exponent = %s", line);

  if (isspace (*line) || *line == '+' || *line == '-' || *line == '\0')
    {
      *degree = 0;
      return line;
    }
  else if (*line != 'x')
    {
      mps_error (ctx, "Unrecognized token after the coefficient: %c", *line);
      return NULL;
    }
  else
    {
      *degree = -1;
      line++;

      if (isspace (*line) || *line == '+' || *line == '-' || *line == '\0')
        *degree = 1;
      else
        {
          if (*line != '^')
            {
              mps_error (ctx, "Unrecognized token after x: %c", *line);
              return NULL;
            }
          else
            {
              errno = 0;
              char *newline;
              *degree = strtol (line + 1, &newline, 10);

              if (errno != 0)
                {
                  mps_error (ctx, "Failed to parse the exponent: %c", *line);
                  return NULL;
                }

              line = newline;
            }
        }
    }

  return line;
}

static char *
parse_sign (mps_context * ctx, char * line, int * sign, mps_boolean *sign_found)
{
  while (isspace (*line) || *line == '-' || *line == '+')
    {
      if (*line == '-')
        *sign *= -1;

      if (*line == '-' || *line == '+')
        {
          *sign_found = true;
        }

      line++;
    }

  return line;
}

static void
update_poly_coefficients (mps_context * ctx,
                          mpq_t ** coefficients_real, mpq_t ** coefficients_imag,
                          int * poly_degree, int degree, mpq_t coefficient_real,
                          mpq_t coefficient_imag)
{
  if (degree > *poly_degree)
    {
      int i;

      *coefficients_real = mps_realloc (*coefficients_real, sizeof(mpq_t) * (degree + 1));
      *coefficients_imag = mps_realloc (*coefficients_imag, sizeof(mpq_t) * (degree + 1));

      for (i = *poly_degree + 1; i <= degree; i++)
        {
          mpq_init ((*coefficients_real)[i]);
          mpq_init ((*coefficients_imag)[i]);

          mpq_set_ui ((*coefficients_real)[i], 0U, 1U);
          mpq_set_ui ((*coefficients_imag)[i], 0U, 1U);
        }

      *poly_degree = degree;
    }

  /* Update the coefficients. We need to "add" instead of "set" since more
   * coefficients of the same degree could be specified more times. */
  mpq_add ((*coefficients_real)[degree],
           (*coefficients_real)[degree],
           coefficient_real);

  mpq_add ((*coefficients_imag)[degree],
           (*coefficients_imag)[degree],
           coefficient_imag);

  if (ctx->debug_level & MPS_DEBUG_IO)
    {
      __MPS_DEBUG (ctx, "Updated coefficient of degree %d: ", degree);
      mpq_out_str (ctx->logstr, 10, (*coefficients_real)[degree]);
      fprintf (ctx->logstr, " + ");
      mpq_out_str (ctx->logstr, 10, (*coefficients_imag)[degree]);
      fprintf (ctx->logstr, "i \n");
    }

  /* In case the leading coefficient has been canceled out by this
   * operation, lower the degree of the polynomial. */
  while ((*poly_degree >= 0) &&
         (mpq_cmp_ui ((*coefficients_real)[*poly_degree], 0U, 1U) == 0) &&
         (mpq_cmp_ui ((*coefficients_imag)[*poly_degree], 0U, 1U) == 0))
    {
      mpq_clear ((*coefficients_real)[*poly_degree]);
      mpq_clear ((*coefficients_imag)[*poly_degree]);

      *coefficients_real = mps_realloc (*coefficients_real, sizeof(mpq_t) * (*poly_degree));
      *coefficients_imag = mps_realloc (*coefficients_imag, sizeof(mpq_t) * (*poly_degree));

      (*poly_degree)--;
    }

  /* Note that there is a small catch in the instructions above: since *poly_degree could
   * go to 0, we may have that *coefficient_real and *coefficient_imag are freed. In that
   * case we shall set the pointers to NULL since it's not guaranteed that realloc will do
   * it. */
  if (*poly_degree == -1)
    {
      *coefficients_real = *coefficients_imag = NULL;
    }

  MPS_DEBUG_WITH_IO (ctx, "The polynomial degree is now = %d", *poly_degree);
}

/**
 * @brief Parse a polynomial described the "usual" way, i.e., written
 * as a_k x^k + a_{k-1} x^{k-1} + ... + a_0.
 *
 * @param ctx The current mps_context.
 * @param stream The input stream that shall be used to read the input
 * polynomial.
 */
MPS_PRIVATE mps_polynomial *
mps_parse_inline_poly_from_stream (mps_context *ctx, mps_abstract_input_stream * stream)
{
  mps_input_buffer * buffer = mps_input_buffer_new (stream);
  int poly_degree = -1;
  int state = PARSING_SIGN;
  int sign = 1, i;

  mpq_t * coefficients_real = NULL;
  mpq_t * coefficients_imag = NULL;

  mps_polynomial *poly = NULL;
  int degree = -1;

  mpq_t current_coefficient_real;
  mpq_t current_coefficient_imag;

  mpq_init (current_coefficient_real);
  mpq_init (current_coefficient_imag);

  char * token = mps_input_buffer_next_token (buffer);
  mps_boolean sign_found = true;

  /* We use original_token to track the token received
   * by mps_input_buffer_next_token() and free it when
   * it's not needed anymore. */
  char * original_token = token;

  /* Start by assuming that we have a list of monomials. Every monomial
   * is of the form [+|-] C x[^K], where
   *
   * C may be a complex number or a real one.
   * K is the exponent, must be a positive integer.
   */
  while (token)
    {
      switch (state)
        {
        case PARSING_ERROR:
          goto cleanup;
          break;

        case PARSING_SIGN:
          token = parse_sign (ctx, token, &sign, &sign_found);

          MPS_DEBUG_WITH_IO (ctx, "Switching sign to %d", sign);

          /* Continuining parsing sign */
          if (*token == '\0')
            state = PARSING_SIGN;
          else
            {
              if (!sign_found)
                {
                  mps_error (ctx, "Missing sign between coefficients");
                  goto cleanup;
                }

              state = PARSING_COEFFICIENT;
            }

          break;

        case PARSING_COEFFICIENT:
          /* We have to distinguish the real from the complex case */
          if (*token == '(')
            {
              /* We need to make sure that we have a sufficiently long token so that
               * all the complex coefficient is here. */
              while (strchr (token, ')') == NULL)
                {
                  char * new_token = mps_input_buffer_next_token (buffer);

                  if (!new_token)
                    {
                      mps_error (ctx, "Cannot find closing bracket for complex coefficient");
                      goto cleanup;
                    }

                  token = mps_realloc (token, strlen (token) + strlen (new_token) + 1);
                  original_token = token;
                  strcat (token, new_token);

                  free (new_token);
                }

              MPS_DEBUG_WITH_IO (ctx, "Complex coefficient = %s", token);

              token = parse_complex_coefficient (ctx, token, current_coefficient_real,
                                                 current_coefficient_imag);
            }
          else
            {
              token = parse_real_coefficient (ctx, token, current_coefficient_real);
              mpq_set_ui (current_coefficient_imag, 0U, 1U);
            }

          if (sign == -1)
            {
              mpq_neg (current_coefficient_real, current_coefficient_real);
              mpq_neg (current_coefficient_imag, current_coefficient_imag);
            }

          if (!token)
            {
              state = PARSING_ERROR;
              goto cleanup;
            }

          if (*token != '\0')
            {
              state = PARSING_EXPONENT;
            }
          else
            {
              /* Degree 0 coefficient */
              degree = 0;

              /* Add the coefficient to the ones of the polynomial */
              update_poly_coefficients (ctx, &coefficients_real, &coefficients_imag,
                                        &poly_degree,
                                        degree, current_coefficient_real,
                                        current_coefficient_imag);

              MPS_DEBUG_WITH_IO (ctx, "Parsed coefficient of degree %d", degree);
              state = PARSING_RESET;
              sign = 1;
            }

          break;

        case PARSING_EXPONENT:
          token = parse_exponent (ctx, token, &degree);
          state = PARSING_RESET;

          if (degree < 0)
            {
              mps_error (ctx, "Degree < 0 in polynomial");
              goto cleanup;
            }

          /* Add the coefficient to the ones of the polynomial */
          update_poly_coefficients (ctx, &coefficients_real, &coefficients_imag,
                                    &poly_degree,
                                    degree, current_coefficient_real,
                                    current_coefficient_imag);

          MPS_DEBUG_WITH_IO (ctx, "Parsed coefficient of degree %d", degree);
          break;

        case PARSING_RESET:
          sign = 1;
          sign_found = false;
          degree = -1;

          state = PARSING_SIGN;

          break;
        }

      if (!token || *token == '\0')
        {
          free (original_token);
          original_token = token = mps_input_buffer_next_token (buffer);
        }
    }

  if (poly_degree < 0)
    goto cleanup;

  MPS_DEBUG_WITH_IO (ctx, "Polynomial degree = %d", poly_degree);

  poly = MPS_POLYNOMIAL (mps_monomial_poly_new (ctx, poly_degree));

  for (i = 0; i <= poly_degree; i++)
    {
      mps_monomial_poly_set_coefficient_q (ctx, MPS_MONOMIAL_POLY (poly), i,
                                           coefficients_real[i],
                                           coefficients_imag[i]);
    }

cleanup:

  free (original_token);

  mps_input_buffer_free (buffer);
  mpq_clear (current_coefficient_real);
  mpq_clear (current_coefficient_imag);

  return poly;
}

/**
 * @brief Parse a polynomial described the "usual" way, i.e., written
 * as a_k x^k + a_{k-1} x^{k-1} + ... + a_0.
 *
 * @param ctx The current mps_context.
 * @param handle A FILE* handle from which the polynomial should be read. 
 */
MPS_PRIVATE mps_polynomial *
mps_parse_inline_poly (mps_context *ctx, FILE * handle)
{
  mps_file_input_stream * stream = mps_file_input_stream_new (handle);
  mps_polynomial * p = mps_parse_inline_poly_from_stream (ctx, (mps_abstract_input_stream*) stream);
  mps_file_input_stream_free (stream);

  return p;
}

/**
 * @brief Parse a polynomial described the "usual" way, i.e., written
 * as a_k x^k + a_{k-1} x^{k-1} + ... + a_0.
 *
 * @param ctx The current mps_context.
 * @param handle A string from which the polynomial should be read. 
 */
mps_polynomial *
mps_parse_inline_poly_from_string (mps_context * ctx, const char * input)
{
  char * input_copy = strdup (input);

  mps_memory_file_stream * stream = mps_memory_file_stream_new (input_copy);
  mps_polynomial * poly = mps_parse_inline_poly_from_stream (ctx, (mps_abstract_input_stream*) stream);

  mps_memory_file_stream_free (stream);
  free (input_copy);
  
  return poly;
}
